#include "probe/capture.hpp"
#include "render/environment.hpp"
#include "scene_workload/allocations.hpp"
#include "scene_workload/fixture.hpp"
#if defined(NGEN_WORKLOAD_INSPECTOR)
#include "tools/reload.hpp"
#include "tools/ui.hpp"
#endif
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>
#include <zlib.h>

namespace {
using namespace ngen::render;
constexpr VkExtent2D extent{1280, 720};
constexpr shader::Matrix identity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
using Clock = std::chrono::steady_clock;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class T> T take(std::variant<T, Failure> result) {
    if (const auto *failure = std::get_if<Failure>(&result))
        throw std::runtime_error(failure->message);
    return std::get<T>(std::move(result));
}
void events() {
    SDL_Event event{};
    for (unsigned count = 0; count < 256 && SDL_PollEvent(&event); ++count)
        require(event.type != SDL_EVENT_QUIT && event.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED,
                "workload cancelled");
}
void wait(Context &context, const Submission &submission) {
    const auto started = SDL_GetTicks();
    while (!take(context.submission_complete(submission))) {
        require(SDL_GetTicks() - started < 10000, "workload GPU completion timed out");
        events();
        SDL_Delay(1);
    }
}
std::uint64_t elapsed(Clock::time_point start) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
}
std::uint32_t number(std::string_view value, std::uint32_t maximum) {
    std::uint32_t result{};
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), result);
    require(parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size() &&
                result <= maximum,
            "invalid workload frame count");
    return result;
}
struct Options {
    std::uint32_t frames{600}, warmup{60};
    bool validation{};
    bool gpu_timings{true};
    bool inspector{}, walkthrough{};
    std::filesystem::path capture, report;
};
Options options(int argc, char **argv) {
    Options result;
    for (int i = 2; i < argc; ++i) {
        const std::string_view argument(argv[i]);
        if (argument.starts_with("--frames="))
            result.frames = number(argument.substr(9), 3600);
        else if (argument.starts_with("--warmup="))
            result.warmup = number(argument.substr(9), 600);
        else if (argument == "--validation")
            result.validation = true;
        else if (argument == "--no-gpu-timings")
            result.gpu_timings = false;
        else if (argument == "--walkthrough")
            result.walkthrough = true;
        else if (argument == "--inspector") {
#if defined(NGEN_WORKLOAD_INSPECTOR)
            result.inspector = true;
#else
            throw std::runtime_error("inspector frontend is unavailable in this build");
#endif
        } else if (argument.starts_with("--capture="))
            result.capture = argument.substr(10);
        else if (argument.starts_with("--report="))
            result.report = argument.substr(9);
        else
            throw std::runtime_error("unknown workload argument");
    }
    require(result.frames > 0, "workload requires at least one measured frame");
    require(!result.walkthrough || (result.inspector && result.frames >= 60),
            "walkthrough requires the inspector and at least 60 measured frames");
    for (const auto &path : {result.capture, result.report})
        require(path.empty() || !std::filesystem::exists(std::filesystem::symlink_status(path)),
                "workload output must be a new path");
    require(result.capture.empty() || result.capture.extension() == ".bmp",
            "capture must use .bmp");
    require(result.report.empty() || result.report.extension() == ".csv", "report must use .csv");
    require(result.capture.empty() || result.report.empty() || result.capture != result.report,
            "workload outputs must be distinct");
    return result;
}
ScenePin resident(Context &context, ngen::assets::PackagePin package) {
    auto upload = take(SceneUpload::begin(context, std::move(package)));
    const auto started = SDL_GetTicks();
    for (;;) {
        auto ready = take(upload->advance(context));
        if (ready)
            return std::move(*ready);
        require(SDL_GetTicks() - started < 60000, "workload scene preparation timed out");
        events();
        SDL_Delay(1);
    }
}
Buffer device_buffer(Context &context, std::span<const std::byte> bytes, VkBufferUsageFlags usage) {
    auto result = take(context.create_buffer(
        {bytes.size(), usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAccess::device}));
    auto staging = take(context.create_buffer(
        {bytes.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAccess::upload}));
    (void)take(staging.write(0, bytes));
    wait(context, take(context.copy_buffer({staging, result, bytes.size()})));
    return result;
}
void upload(Context &context, const Image &target, std::span<const std::byte> bytes) {
    auto staging = take(context.create_buffer(
        {bytes.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAccess::upload}));
    (void)take(staging.write(0, bytes));
    wait(context, take(context.copy_to_image({staging, target})));
}
struct Sample {
    std::uint64_t frame_ns{}, prepare_ns{}, present_ns{}, ui_prepare_ns{};
    Submission mesh, post;
    GpuTimings gpu;
    bool gpu_ready{};
};
std::uint64_t count_ship_triangles(const ngen::assets::Scene &package, std::size_t asteroids) {
    std::uint64_t total = 0;
    for (const auto &instance : package.instances) {
        const auto &lods = package.primitives[instance.primitive].lods;
        total += lods[std::min(std::size_t{2}, lods.size() - 1)].index_count / 3;
    }
    require(total + asteroids <= 250000 && !package.instances.empty() &&
                package.instances.size() < 256,
            "laptop workload exceeds the 250000-triangle/256-draw profile");
    return total;
}
std::uint32_t fixture_crc(const ngen::workload::Fixture &fixture) {
    auto value = crc32(0, nullptr, 0);
    const auto add = [&](const auto &array) {
        const auto bytes = std::as_bytes(std::span(array));
        value = crc32_z(value, reinterpret_cast<const Bytef *>(bytes.data()), bytes.size());
    };
    add(fixture.vertices);
    add(fixture.indices);
    add(fixture.instances);
    return static_cast<std::uint32_t>(value);
}
} // namespace

int main(int argc, char **argv) try {
    if (argc < 2) {
        std::fputs("Usage: scene_workload scene.ngas [--frames=N] [--warmup=N] [--validation] "
                   "[--no-gpu-timings] [--inspector] [--capture=new.bmp] [--report=new.csv]\n",
                   stderr);
        return 2;
    }
    Options settings;
    try {
        settings = options(argc, argv);
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Workload arguments: %s\n", error.what());
        return 2;
    }
    auto loaded = ngen::assets::load_package_file(argv[1]);
    if (const auto *failure = std::get_if<ngen::assets::Failure>(&loaded))
        throw std::runtime_error(failure->message);
    auto package = std::get<ngen::assets::PackagePin>(std::move(loaded));
    auto fixture = take(ngen::workload::make_fixture(package->bounds));
    const auto asteroid_triangles = fixture.indices.size() / 3 * fixture.instances.size();
    auto ship_triangles = count_ship_triangles(*package, asteroid_triangles);
    auto context = take(Context::create({settings.validation, true, true, settings.gpu_timings}));
    require(SDL_SetWindowSize(context->window(), static_cast<int>(extent.width),
                              static_cast<int>(extent.height)),
            "workload window resize failed");
    auto version = resident(*context, package);
    auto vertices = device_buffer(*context, std::as_bytes(std::span(fixture.vertices)),
                                  VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    auto indices = device_buffer(*context, std::as_bytes(std::span(fixture.indices)),
                                 VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    auto color = take(
        context->create_image({extent, VK_FORMAT_R16G16B16A16_SFLOAT,
                               static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT}));
    auto depth = take(context->create_image(
        {extent, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT}));
    auto shadow = take(context->create_image(
        {{1024, 1024},
         VK_FORMAT_D32_SFLOAT,
         static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) |
             VK_IMAGE_USAGE_SAMPLED_BIT}));
    auto white =
        take(context->create_image({{1, 1},
                                    VK_FORMAT_R8G8B8A8_UNORM,
                                    static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT}));
    const std::array<std::byte, 4> white_pixel{std::byte{255}, std::byte{255}, std::byte{255},
                                               std::byte{255}};
    upload(*context, white, white_pixel);
    const auto generated = take(generate_environment(Sky{}));
    std::array<Image, 3> environment;
    for (std::size_t i = 0; i < environment.size(); ++i) {
        environment[i] = take(context->create_image(generated.images[i].description));
        upload(*context, environment[i], generated.images[i].texels);
    }
    const SceneTargets targets{color,          depth,          shadow, environment[0],
                               environment[1], environment[2], white};
    MeshDraw asteroids{};
    asteroids.frame.model = asteroids.frame.normalMatrix = identity;
    asteroids.material.baseColor = {.23F, .19F, .15F, 1};
    asteroids.material.metallicRoughnessNormalOcclusion = {.05F, .95F, 1, 1};
    asteroids.material.flags[2] = 1;
    asteroids.maps.fill(&white);
    asteroids.index_count = static_cast<std::uint32_t>(fixture.indices.size());
    asteroids.instances = fixture.instances;
    asteroids.vertices = &vertices;
    asteroids.indices = &indices;
    SceneRenderer renderer;
    float exposure = 1;
    bool fxaa = true;
#if defined(NGEN_WORKLOAD_INSPECTOR)
    std::unique_ptr<ngen::tools::Inspector> inspector;
    std::unique_ptr<ngen::tools::InspectorUi> ui;
    std::unique_ptr<ngen::tools::PackageReloader> reloader;
    std::unique_ptr<SceneUpload> replacement;
    std::optional<ngen::workload::Fixture> replacement_fixture;
    std::uint64_t requested_reload{}, completed_reloads{}, replacement_triangles{},
        scripted_reload{};
    bool measuring{}, walkthrough_edits{};
    unsigned walkthrough_resizes = 0, walkthrough_resize_mask = 0;
    ngen::tools::InspectorMetrics metrics{};
    std::string reload_status("Immutable snapshot / queued edits apply next frame");
    if (settings.inspector) {
        auto created = ngen::tools::Inspector::create(package);
        require(std::holds_alternative<ngen::tools::Inspector>(created),
                "inspector package admission failed");
        inspector = std::make_unique<ngen::tools::Inspector>(
            std::get<ngen::tools::Inspector>(std::move(created)));
        require(std::holds_alternative<std::monostate>(
                    inspector->enqueue(ngen::tools::Select{inspector->snapshot().generation, 0})),
                "initial inspector selection failed");
        (void)inspector->advance();
        ui = take(ngen::tools::InspectorUi::create(*context));
        reloader = std::make_unique<ngen::tools::PackageReloader>(std::filesystem::path(argv[1]));
        metrics.status = "Immutable snapshot / queued edits apply next frame";
    }
#endif
    const auto pump = [&] {
        SDL_Event event{};
        for (unsigned count = 0; count < 256 && SDL_PollEvent(&event); ++count) {
#if defined(NGEN_WORKLOAD_INSPECTOR)
            if (ui)
                ui->event(event);
#endif
            require(event.type != SDL_EVENT_QUIT && event.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED,
                    "workload cancelled");
        }
    };
#if defined(NGEN_WORKLOAD_INSPECTOR)
    const auto update_replacement = [&] {
        if (!ui)
            return;
        // Native ownership stays on this thread. The worker only produces an
        // owning checked CPU package; no partially uploaded version is published.
        if (replacement) {
            auto progress = replacement->advance(*context);
            if (const auto *failure = std::get_if<Failure>(&progress)) {
                reload_status = "GPU reload rejected: " + failure->message;
                replacement.reset();
                replacement_fixture.reset();
            } else if (auto &ready = std::get<std::optional<ScenePin>>(progress); ready) {
                auto staged = inspector->stage_package((*ready)->package());
                if (std::holds_alternative<std::monostate>(staged)) {
                    version = std::move(*ready);
                    package = version->package();
                    fixture = replacement_fixture.value();
                    asteroids.instances = fixture.instances;
                    ship_triangles = replacement_triangles;
                    ++completed_reloads;
                    reload_status =
                        "Completed CPU/GPU version published; old frame pins remain valid";
                } else {
                    reload_status =
                        "Reload generation/capacity rejected; previous version retained";
                }
                replacement.reset();
                replacement_fixture.reset();
            }
        }
        if (auto outcome = reloader->poll()) {
            if (const auto *failure = std::get_if<ngen::assets::Failure>(&outcome->package)) {
                reload_status = "CPU reload rejected (issue " +
                                std::to_string(static_cast<unsigned>(failure->issue)) +
                                "): " + failure->message;
            } else if (outcome->request != requested_reload) {
                reload_status = "Stale reload result rejected; previous version retained";
            } else {
                auto candidate = std::get<ngen::assets::PackagePin>(std::move(outcome->package));
                try {
                    const auto candidate_triangles =
                        count_ship_triangles(*candidate, asteroid_triangles);
                    auto candidate_fixture = take(ngen::workload::make_fixture(candidate->bounds));
                    auto uploading = take(SceneUpload::begin(*context, std::move(candidate)));
                    replacement = std::move(uploading);
                    replacement_fixture = candidate_fixture;
                    replacement_triangles = candidate_triangles;
                    reload_status = "Checked CPU package ready / tracked sequential GPU upload";
                } catch (const std::exception &error) {
                    reload_status = "Reload admission rejected: " + std::string(error.what());
                }
            }
        }
        const auto advanced = inspector->advance();
        if (advanced.rejected)
            reload_status = "Stale queued commands rejected at version publication";
        const auto snapshot = inspector->snapshot();
        if (snapshot.reload_request > requested_reload && !replacement) {
            const auto requested = reloader->request(snapshot.reload_request);
            if (std::holds_alternative<std::monostate>(requested)) {
                requested_reload = snapshot.reload_request;
                reload_status = "Reading checked package on dedicated cancellable I/O worker";
            } else if (std::get<ngen::Error>(requested) == ngen::Error::busy) {
                reload_status = "Reload worker busy; request deferred";
            } else {
                reload_status = "Reload request admission rejected; previous version retained";
            }
        }
        metrics.reloading = replacement != nullptr || reloader->busy();
        metrics.status = reload_status;
    };
#endif
    std::vector<Sample> samples(settings.frames);
    Submission last;
    const auto frame = [&](std::uint32_t index) {
        const auto started = Clock::now();
        pump();
        const auto admission = SDL_GetTicks();
        while (take(renderer.collect(*context)) >= 2) {
            require(SDL_GetTicks() - admission < 10000, "workload frame pin admission timed out");
            pump();
            SDL_Delay(1);
        }
#if defined(NGEN_WORKLOAD_INSPECTOR)
        update_replacement();
        if (ui && settings.walkthrough && measuring && (index == 20 || index == 40)) {
            const auto width = index == 20 ? 1024 : 1280;
            const auto height = index == 20 ? 640 : 720;
            require(SDL_SetWindowSize(context->window(), width, height),
                    "inspector walkthrough resize failed");
            ++walkthrough_resizes;
            pump();
        }
#endif
        auto selected = take(ngen::workload::view(fixture, index, extent));
        std::uint64_t ui_prepare_ns = 0;
        const Overlay *overlay = nullptr;
#if defined(NGEN_WORKLOAD_INSPECTOR)
        ngen::tools::InspectorSnapshot snapshot;
        const auto ui_started = Clock::now();
        if (ui) {
            snapshot = inspector->snapshot();
            selected.material_factors =
                std::span(snapshot.materials).first(snapshot.material_count);
            exposure = snapshot.view.exposure;
            fxaa = snapshot.view.fxaa;
            metrics.frame = index;
            if (index % 30 == 0)
                metrics.memory = take(context->resource_statistics());
            ui->frame(*inspector, snapshot, selected, metrics);
            if (settings.walkthrough && measuring) {
                if (index == 0) {
                    const auto instance = static_cast<std::uint32_t>(
                        std::min(std::size_t{1}, package->instances.size() - 1));
                    const auto material =
                        package->primitives[package->instances[instance].primitive].material;
                    require(material < snapshot.material_count,
                            "walkthrough requires an authored material");
                    auto factors = snapshot.materials[material];
                    factors.roughness = .25F;
                    for (const auto &command :
                         {ngen::tools::InspectorCommand{
                              ngen::tools::Select{snapshot.generation, instance}},
                          ngen::tools::InspectorCommand{
                              ngen::tools::EditMaterial{snapshot.generation, material, factors}},
                          ngen::tools::InspectorCommand{
                              ngen::tools::SetView{snapshot.generation, {.75F, false}}}})
                        require(std::holds_alternative<std::monostate>(inspector->enqueue(command)),
                                "walkthrough command rejected");
                }
                if (index == 1) {
                    require(snapshot.selection.has_value(),
                            "walkthrough selection did not apply next frame");
                    const auto material =
                        package
                            ->primitives[package->instances[snapshot.selection.value()].primitive]
                            .material;
                    require(snapshot.materials[material].roughness == .25F &&
                                snapshot.view.exposure == .75F && !snapshot.view.fxaa,
                            "walkthrough factors/view did not apply next frame");
                    walkthrough_edits = true;
                }
                if (index == 5) {
                    scripted_reload = snapshot.reload_request;
                    SDL_Event key{};
                    key.type = SDL_EVENT_KEY_DOWN;
                    key.key.windowID = SDL_GetWindowID(context->window());
                    key.key.scancode = SDL_SCANCODE_R;
                    key.key.key = SDLK_R;
                    key.key.down = true;
                    require(SDL_PushEvent(&key), "walkthrough key-down event rejected");
                    key.type = SDL_EVENT_KEY_UP;
                    key.key.down = false;
                    require(SDL_PushEvent(&key), "walkthrough key-up event rejected");
                }
            }
        }
        auto geometry = [&]() -> std::optional<Overlay> {
            if (!ui)
                return {};
            int width = 0, height = 0;
            require(SDL_GetWindowSizeInPixels(context->window(), &width, &height) && width > 0 &&
                        height > 0,
                    "inspector drawable is unavailable");
            return take(ui->overlay({static_cast<unsigned>(width), static_cast<unsigned>(height)}));
        }();
        if (geometry)
            overlay = &*geometry;
        if (ui)
            ui_prepare_ns = elapsed(ui_started);
#endif
        const auto prepared = Clock::now();
        const auto mesh = [&] {
            for (;;) {
                auto result = renderer.draw(*context, version, selected, targets, {},
                                            std::span{&asteroids, 1});
                const auto *failure = std::get_if<Failure>(&result);
                if (!failure || (failure->message != "transfer contexts are busy" &&
                                 failure->message != "scene frame pins are busy"))
                    return take(std::move(result));
                require(SDL_GetTicks() - admission < 10000,
                        "workload mesh/upload admission timed out");
                (void)take(renderer.collect(*context));
                pump();
                SDL_Delay(1);
            }
        }();
        const auto prepare_ns = elapsed(prepared);
        const auto presenting = Clock::now();
        for (;;) {
            const auto result =
                take(context->render_frame(ImageFrame{color, exposure, fxaa, overlay}));
            if (result.outcome == FrameOutcome::queued) {
                last = result.resources;
#if defined(NGEN_WORKLOAD_INSPECTOR)
                if (ui && settings.walkthrough && measuring) {
                    const auto state = context->presentation_state();
                    if (index >= 20 && index < 40 && state.extent.width == 1024 &&
                        state.extent.height == 640)
                        walkthrough_resize_mask |= 1U;
                    if (index >= 40 && (walkthrough_resize_mask & 1U) &&
                        state.extent.width == 1280 && state.extent.height == 720)
                        walkthrough_resize_mask |= 2U;
                }
#endif
                break;
            }
            require(SDL_GetTicks() - admission < 10000, "workload WSI admission timed out");
            pump();
            SDL_Delay(1);
        }
        return Sample{elapsed(started),
                      prepare_ns,
                      elapsed(presenting),
                      ui_prepare_ns,
                      mesh,
                      last,
                      {},
                      false};
    };
    for (std::uint32_t i = 0; i < settings.warmup; ++i) {
        const auto warmed = frame(i);
#if defined(NGEN_WORKLOAD_INSPECTOR)
        metrics.frame_ms = static_cast<double>(warmed.frame_ns) / 1e6;
#else
        (void)warmed;
#endif
    }
    std::size_t collected = 0;
    const auto collect_samples = [&](std::size_t submitted) {
        while (collected < submitted) {
            auto &sample = samples[collected];
            if (!take(context->submission_complete(sample.post)))
                break;
            const auto mesh = take(context->submission_timings(sample.mesh));
            const auto post = take(context->submission_timings(sample.post));
            if (mesh && post) {
                sample.gpu = {mesh->shadow_ns, mesh->pbr_ns, post->post_ns, post->ui_ns};
                sample.gpu_ready = true;
#if defined(NGEN_WORKLOAD_INSPECTOR)
                metrics.gpu = sample.gpu;
#endif
            }
            ++collected;
        }
    };
#if defined(NGEN_WORKLOAD_INSPECTOR)
    measuring = true;
    const auto generation_start = context->presentation_state().generation;
    const auto ui_allocation_start = ui ? ui->allocation_count() : 0;
#endif
    const auto allocation_start = ngen::workload::allocation_count();
    for (std::uint32_t i = 0; i < settings.frames; ++i) {
        const auto started = Clock::now();
        samples[i] = frame(i);
        collect_samples(static_cast<std::size_t>(i) + 1);
        samples[i].frame_ns = elapsed(started); // Include diagnostics/recycling in owner-loop cost.
#if defined(NGEN_WORKLOAD_INSPECTOR)
        metrics.frame_ms = static_cast<double>(samples[i].frame_ns) / 1e6;
#endif
    }
    const auto allocations = ngen::workload::allocation_count() - allocation_start;
#if defined(NGEN_WORKLOAD_INSPECTOR)
    const auto ui_allocations = ui ? ui->allocation_count() - ui_allocation_start : 0;
#endif
    wait(*context, last);
    collect_samples(samples.size());
    const bool gpu_available = context->gpu_timings_available() &&
                               std::all_of(samples.begin(), samples.end(),
                                           [](const Sample &sample) { return sample.gpu_ready; });
    require(take(renderer.collect(*context)) == 0, "workload completed scene pins did not retire");
    const auto statistics = take(context->resource_statistics());
    auto readback =
        take(context->create_buffer({VkDeviceSize{extent.width} * extent.height * 8,
                                     VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAccess::readback}));
    wait(*context, take(context->copy_from_image({color, readback})));
    std::vector<std::uint16_t> hdr(std::size_t{extent.width} * extent.height * 4);
    (void)take(readback.read(0, std::as_writable_bytes(std::span(hdr))));
    std::size_t lit = 0;
    for (std::size_t i = 0; i < hdr.size(); i += 4) {
        bool nonzero = false;
        for (std::size_t channel = 0; channel < 4; ++channel) {
            require((hdr[i + channel] & 0x7c00U) != 0x7c00U, "workload HDR output is nonfinite");
            nonzero = nonzero || (channel < 3 && hdr[i + channel] != 0);
        }
        lit += nonzero ? 1U : 0U;
    }
    require(lit > 0, "workload HDR output is blank");
    if (!settings.capture.empty()) {
        const Overlay *overlay = nullptr;
#if defined(NGEN_WORKLOAD_INSPECTOR)
        auto geometry = ui ? take(ui->overlay(extent)) : std::optional<Overlay>{};
        if (geometry)
            overlay = &*geometry;
#endif
        (void)take(ngen::probe::capture(*context, color, extent, settings.capture, exposure, fxaa,
                                        overlay));
    }
    if (!settings.report.empty()) {
        std::string report("frame,frame_ns,prepare_ns,present_ns,gpu_shadow_ns,gpu_pbr_ns,gpu_post_"
                           "ns,ui_prepare_ns,gpu_ui_ns\n");
        report.reserve(samples.size() * 80);
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const auto &sample = samples[i];
            report += std::to_string(i) + ',' + std::to_string(sample.frame_ns) + ',' +
                      std::to_string(sample.prepare_ns) + ',' + std::to_string(sample.present_ns) +
                      ',' + std::to_string(sample.gpu.shadow_ns) + ',' +
                      std::to_string(sample.gpu.pbr_ns) + ',' + std::to_string(sample.gpu.post_ns) +
                      ',' + std::to_string(sample.ui_prepare_ns) + ',' +
                      std::to_string(sample.gpu.ui_ns) + '\n';
        }
        (void)take(ngen::probe::write_exclusive(settings.report, std::as_bytes(std::span(report))));
    }
    std::sort(samples.begin(), samples.end(),
              [](const Sample &a, const Sample &b) { return a.frame_ns < b.frame_ns; });
    const auto percentile = [&](std::size_t percent) {
        return samples[(samples.size() * percent + 99) / 100 - 1].frame_ns;
    };
    std::printf(
        "asteroid_instances=%zu asteroid_triangles=%zu ship_triangles=%llu total_triangles=%llu "
        "scene_draws=%zu fixture_seed=0x%08x fixture_crc32=%08x package_crc32=%08x\n",
        fixture.instances.size(), fixture.indices.size() / 3,
        static_cast<unsigned long long>(ship_triangles),
        static_cast<unsigned long long>(ship_triangles) +
            static_cast<unsigned long long>(asteroid_triangles),
        package->instances.size() + 1, fixture.seed, fixture_crc(fixture), package->checksum);
    std::printf(
        "workload_frames=%u workload_warmup=%u workload_wsi_queued=%u workload_hdr_finite=1 "
        "workload_lit_pixels=%zu workload_capture=%u width=1280 height=720 shadow_extent=1024 "
        "validation=%u\n",
        settings.frames, settings.warmup, settings.frames, lit, settings.capture.empty() ? 0U : 1U,
        settings.validation ? 1U : 0U);
    std::printf("frame_ns p50=%llu p95=%llu p99=%llu max=%llu "
                "gpu_timestamps=%s allocation_probe=%u warm_cpp_allocations=%llu "
                "vma_allocation_bytes=%llu vma_reserved_bytes=%llu\n",
                static_cast<unsigned long long>(percentile(50)),
                static_cast<unsigned long long>(percentile(95)),
                static_cast<unsigned long long>(percentile(99)),
                static_cast<unsigned long long>(samples.back().frame_ns),
                gpu_available ? "available" : "unavailable",
                ngen::workload::allocation_probe_enabled() ? 1U : 0U,
                static_cast<unsigned long long>(allocations),
                static_cast<unsigned long long>(statistics.allocation_bytes),
                static_cast<unsigned long long>(statistics.reserved_bytes));
    const auto distribution = [&](const auto &select, std::size_t percent) {
        std::vector<double> values;
        values.reserve(samples.size());
        for (const auto &sample : samples)
            values.push_back(static_cast<double>(select(sample)));
        std::sort(values.begin(), values.end());
        return values[(values.size() * percent + 99) / 100 - 1];
    };
    std::printf("cpu_prepare_ns p50=%.0f p95=%.0f cpu_present_ns p50=%.0f p95=%.0f\n",
                distribution([](const Sample &sample) { return sample.prepare_ns; }, 50),
                distribution([](const Sample &sample) { return sample.prepare_ns; }, 95),
                distribution([](const Sample &sample) { return sample.present_ns; }, 50),
                distribution([](const Sample &sample) { return sample.present_ns; }, 95));
    if (gpu_available)
        std::printf("gpu_shadow_ns p50=%.0f p95=%.0f gpu_pbr_ns p50=%.0f p95=%.0f "
                    "gpu_post_ns p50=%.0f p95=%.0f\n",
                    distribution([](const Sample &sample) { return sample.gpu.shadow_ns; }, 50),
                    distribution([](const Sample &sample) { return sample.gpu.shadow_ns; }, 95),
                    distribution([](const Sample &sample) { return sample.gpu.pbr_ns; }, 50),
                    distribution([](const Sample &sample) { return sample.gpu.pbr_ns; }, 95),
                    distribution([](const Sample &sample) { return sample.gpu.post_ns; }, 50),
                    distribution([](const Sample &sample) { return sample.gpu.post_ns; }, 95));
#if defined(NGEN_WORKLOAD_INSPECTOR)
    if (ui) {
        const auto snapshot = inspector->snapshot();
        if (settings.walkthrough) {
            std::printf(
                "walkthrough_edits=%u walkthrough_keyboard_reload=%u inspector_resize_mask=%u "
                "inspector_native_generations=%llu inspector_reload_phase=%s "
                "inspector_completed_reloads=%llu\n",
                walkthrough_edits ? 1U : 0U, requested_reload > scripted_reload ? 1U : 0U,
                walkthrough_resize_mask,
                static_cast<unsigned long long>(context->presentation_state().generation -
                                                generation_start),
                replacement        ? "gpu"
                : reloader->busy() ? "cpu"
                                   : "idle",
                static_cast<unsigned long long>(completed_reloads));
            require(walkthrough_edits && requested_reload > scripted_reload &&
                        completed_reloads == 1 && walkthrough_resizes == 2 &&
                        walkthrough_resize_mask == 3 &&
                        context->presentation_state().generation >= generation_start + 2,
                    "inspector walkthrough did not complete edits, keyboard reload and native "
                    "resize publication");
            std::printf("walkthrough_edits=1 walkthrough_keyboard_reload=1 inspector_resizes=2 ");
        }
        std::printf("inspector_reloads=%llu ", static_cast<unsigned long long>(completed_reloads));
        const auto geometry = take(ui->overlay(extent));
        std::printf("inspector_ui=1 inspector_snapshot_generation=%llu inspector_vertices=%zu "
                    "inspector_draws=%zu "
                    "warm_imgui_allocations=%llu ui_prepare_ns_p95=%.0f gpu_ui_ns_p95=%.0f\n",
                    static_cast<unsigned long long>(snapshot.generation),
                    geometry ? geometry->vertices.size() : 0, geometry ? geometry->draws.size() : 0,
                    static_cast<unsigned long long>(ui_allocations),
                    distribution([](const Sample &sample) { return sample.ui_prepare_ns; }, 95),
                    distribution([](const Sample &sample) { return sample.gpu.ui_ns; }, 95));
        reloader->cancel();
        replacement.reset();
        reloader.reset(); // Stop/join I/O before native owner teardown.
        ui.reset();
    }
#endif
    require(context->close(), "workload context close/validation failed");
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "Scene workload: %s\n", error.what());
    return 1;
}
