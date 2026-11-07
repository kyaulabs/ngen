#include "probe/capture.hpp"
#include "render/environment.hpp"
#include "scene_workload/allocations.hpp"
#include "scene_workload/fixture.hpp"
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
    while (SDL_PollEvent(&event))
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
        else if (argument.starts_with("--capture="))
            result.capture = argument.substr(10);
        else if (argument.starts_with("--report="))
            result.report = argument.substr(9);
        else
            throw std::runtime_error("unknown workload argument");
    }
    require(result.frames > 0, "workload requires at least one measured frame");
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
    std::uint64_t frame_ns{}, prepare_ns{}, present_ns{};
    Submission mesh, post;
    GpuTimings gpu;
    bool gpu_ready{};
};
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
                   "[--no-gpu-timings] [--capture=new.bmp] [--report=new.csv]\n",
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
    const auto fixture = take(ngen::workload::make_fixture(package->bounds));
    std::uint64_t ship_triangles = 0;
    for (const auto &instance : package->instances) {
        const auto &lods = package->primitives[instance.primitive].lods;
        ship_triangles += lods[std::min(std::size_t{2}, lods.size() - 1)].index_count / 3;
    }
    const auto triangles = ship_triangles + fixture.indices.size() / 3 * fixture.instances.size();
    require(triangles <= 250000 && package->instances.size() < 256,
            "laptop workload exceeds the 250000-triangle/256-draw profile");
    auto context = take(Context::create({settings.validation, true, true, settings.gpu_timings}));
    require(SDL_SetWindowSize(context->window(), static_cast<int>(extent.width),
                              static_cast<int>(extent.height)),
            "workload window resize failed");
    const auto version = resident(*context, package);
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
    std::vector<Sample> samples(settings.frames);
    Submission last;
    const auto frame = [&](std::uint32_t index) {
        const auto started = Clock::now();
        events();
        const auto admission = SDL_GetTicks();
        while (take(renderer.collect(*context)) >= 2) {
            require(SDL_GetTicks() - admission < 10000, "workload frame pin admission timed out");
            events();
            SDL_Delay(1);
        }
        const auto selected = take(ngen::workload::view(fixture, index, extent));
        const auto prepared = Clock::now();
        const auto mesh =
            take(renderer.draw(*context, version, selected, targets, {}, std::span{&asteroids, 1}));
        const auto prepare_ns = elapsed(prepared);
        const auto presenting = Clock::now();
        for (;;) {
            const auto result = take(context->render_frame(ImageFrame{color}));
            if (result.outcome == FrameOutcome::queued) {
                last = result.resources;
                break;
            }
            require(SDL_GetTicks() - admission < 10000, "workload WSI admission timed out");
            events();
            SDL_Delay(1);
        }
        return Sample{elapsed(started), prepare_ns, elapsed(presenting), mesh, last, {}, false};
    };
    for (std::uint32_t i = 0; i < settings.warmup; ++i)
        (void)frame(i);
    std::size_t collected = 0;
    const auto collect_samples = [&](std::size_t submitted) {
        while (collected < submitted) {
            auto &sample = samples[collected];
            if (!take(context->submission_complete(sample.post)))
                break;
            const auto mesh = take(context->submission_timings(sample.mesh));
            const auto post = take(context->submission_timings(sample.post));
            if (mesh && post) {
                sample.gpu = {mesh->shadow_ns, mesh->pbr_ns, post->post_ns};
                sample.gpu_ready = true;
            }
            ++collected;
        }
    };
    const auto allocation_start = ngen::workload::allocation_count();
    for (std::uint32_t i = 0; i < settings.frames; ++i) {
        const auto started = Clock::now();
        samples[i] = frame(i);
        collect_samples(static_cast<std::size_t>(i) + 1);
        samples[i].frame_ns = elapsed(started); // Include diagnostics/recycling in owner-loop cost.
    }
    const auto allocations = ngen::workload::allocation_count() - allocation_start;
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
    if (!settings.capture.empty())
        (void)take(ngen::probe::capture(*context, color, extent, settings.capture));
    if (!settings.report.empty()) {
        std::string report(
            "frame,frame_ns,prepare_ns,present_ns,gpu_shadow_ns,gpu_pbr_ns,gpu_post_ns\n");
        report.reserve(samples.size() * 80);
        for (std::size_t i = 0; i < samples.size(); ++i) {
            const auto &sample = samples[i];
            report += std::to_string(i) + ',' + std::to_string(sample.frame_ns) + ',' +
                      std::to_string(sample.prepare_ns) + ',' + std::to_string(sample.present_ns) +
                      ',' + std::to_string(sample.gpu.shadow_ns) + ',' +
                      std::to_string(sample.gpu.pbr_ns) + ',' + std::to_string(sample.gpu.post_ns) +
                      '\n';
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
        static_cast<unsigned long long>(ship_triangles), static_cast<unsigned long long>(triangles),
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
    require(context->close(), "workload context close/validation failed");
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "Scene workload: %s\n", error.what());
    return 1;
}
