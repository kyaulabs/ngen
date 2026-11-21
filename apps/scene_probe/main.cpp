#include "draw.hpp"
#include "package_fixture.hpp"
#include "render/scene.hpp"
#ifdef NGEN_SCENE_PROBE_PHYSICS
#include "tools/reload.hpp"
#include "travel/destination.hpp"
#include "travel/system_manifest.hpp"
#endif
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <exception>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <thread>

namespace {
bool failed(const auto &result, const char *operation) {
    if (const auto *failure = std::get_if<ngen::render::Failure>(&result)) {
        std::fprintf(stderr, "Scene probe %s: %s\n", operation, failure->message.c_str());
        return true;
    }
    return false;
}
std::variant<ngen::render::ScenePin, ngen::render::Failure>
finish(ngen::render::Context &context, ngen::render::SceneUpload &upload) {
    const auto start = SDL_GetTicks();
    for (;;) {
        const auto progress = upload.advance(context);
        if (const auto *failure = std::get_if<ngen::render::Failure>(&progress))
            return *failure;
        if (const auto &ready = std::get<std::optional<ngen::render::ScenePin>>(progress))
            return *ready;
        if (SDL_GetTicks() - start >= 5000)
            return ngen::render::Failure{"scene upload timed out"};
        SDL_Delay(1);
    }
}
class Gate {
  public:
    explicit Gate(ngen::render::Context &context) : context_(context) {
        VkSemaphoreTypeCreateInfo type{};
        type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = &type;
        if (vkCreateSemaphore(context_.device(), &info, nullptr, &handle) != VK_SUCCESS)
            throw std::runtime_error("scene gate creation failed");
    }
    ~Gate() {
        if (!signal() || (used && !done && vkQueueWaitIdle(context_.queue()) != VK_SUCCESS))
            return;
        vkDestroySemaphore(context_.device(), handle, nullptr);
    }
    Gate(const Gate &) = delete;
    Gate &operator=(const Gate &) = delete;
    bool signal() {
        if (signaled_)
            return true;
        VkSemaphoreSignalInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        info.semaphore = handle;
        info.value = 1;
        signaled_ = vkSignalSemaphore(context_.device(), &info) == VK_SUCCESS;
        return signaled_;
    }
    VkSemaphore handle{};
    bool used{}, done{};

  private:
    ngen::render::Context &context_;
    bool signaled_{};
};
bool cancel_inflight(ngen::render::Context &context, const ngen::render::ScenePin &active) {
    auto a = context.create_buffer(
        {16, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
    auto b = context.create_buffer({16, VK_BUFFER_USAGE_TRANSFER_DST_BIT});
    if (failed(a, "gate source") || failed(b, "gate target"))
        return false;
    auto source = std::get<ngen::render::Buffer>(std::move(a));
    auto target = std::get<ngen::render::Buffer>(std::move(b));
    const std::array<std::byte, 16> bytes{};
    if (failed(source.write(0, bytes), "gate bytes"))
        return false;
    auto pending = ngen::render::SceneUpload::begin(context, active->package());
    if (failed(pending, "in-flight replacement admission"))
        return false;
    auto upload = std::get<std::unique_ptr<ngen::render::SceneUpload>>(std::move(pending));
    Gate gate(context);
    if (failed(context.copy_buffer({source, target, 16, {gate.handle, 1}}), "scene gate submit"))
        return false;
    gate.used = true;
    const auto queued = upload->advance(context);
    if (failed(queued, "queued scene phase") ||
        std::get<std::optional<ngen::render::ScenePin>>(queued))
        return false;
    std::stop_source stop;
    (void)stop.request_stop();
    const auto cancelled = upload->advance(context, stop.get_token());
    if (!std::holds_alternative<ngen::render::Failure>(cancelled) || !active->vertices().native())
        return false;
    upload.reset();
    const auto held = context.resource_statistics();
    if (failed(held, "cancelled pending resources") ||
        std::get<ngen::render::ResourceStatistics>(held).retiring != 2) {
        std::fputs("Cancelled scene did not retain its submitted staging/geometry\n", stderr);
        return false;
    }
    if (!gate.signal())
        return false;
    const auto start = SDL_GetTicks();
    for (;;) {
        const auto statistics = context.resource_statistics();
        if (failed(statistics, "cancel retirement"))
            return false;
        if (std::get<ngen::render::ResourceStatistics>(statistics).retiring == 0)
            break;
        if (SDL_GetTicks() - start >= 2000)
            return false;
        SDL_Delay(1);
    }
    gate.done = true;
    std::puts("cancelled_inflight_scene=1 unpublished_version=1 active_pin_preserved=1 "
              "submitted_retirement=1");
    return true;
}
#ifdef NGEN_SCENE_PROBE_PHYSICS
using CrossingFiles = std::array<std::filesystem::path, 3>;

ngen::assets::PackageResult collect_destination(ngen::tools::PackageReloader &worker) {
    const auto start = SDL_GetTicks();
    for (;;) {
        if (auto result = worker.poll())
            return std::move(result->package);
        if (SDL_GetTicks() - start >= 2000)
            throw std::runtime_error("destination read did not respond");
        SDL_Delay(1);
    }
}

bool crossing(ngen::render::Context &context, const ngen::render::ScenePin &proxy,
              const std::optional<CrossingFiles> &files, std::uint64_t *sampled_bytes = nullptr) {
    const auto sample = [&] {
        if (!sampled_bytes)
            return true;
        const auto stats = context.resource_statistics();
        if (failed(stats, "crossing resource sample"))
            return false;
        const auto bytes = std::get<ngen::render::ResourceStatistics>(stats).allocation_bytes;
        *sampled_bytes = std::max(*sampled_bytes, bytes);
        return bytes <= ngen::render::resource_allocation_limit;
    };
    using namespace ngen;
    const auto systems = travel::streaming_fixture(19);
    const auto route = std::get<travel::Route>(systems.gate(systems.starter_system, 2, true));
    auto journey = std::get<std::unique_ptr<travel::Journey>>(travel::Journey::create(route, {}));
    travel::DestinationCollision destination(route);
    render::ScenePrefetch prefetch(proxy);
    std::atomic<bool> release_read{false};
    std::optional<tools::PackageReloader> first_read;
    if (files) {
        first_read.emplace(files->front(), [&](const auto &path, const std::stop_token &stop) {
            while (!release_read.load() && !stop.stop_requested())
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            return assets::load_package_file(path, stop);
        });
        if (!std::holds_alternative<std::monostate>(first_read->request(1)))
            return false;
    }
    const auto departure = journey->snapshot();
    if (!std::holds_alternative<travel::Snapshot>(
            journey->begin({departure.vessel, departure.tick, 1})))
        return false;
    if (!files && (prefetch.begin(context, std::get<assets::PackagePin>(
                                               assets::load_package(package_fixture::encode()))) ||
                   prefetch.advance(context)))
        return false;
    for (std::uint32_t tick = 0; tick <= route.duration_ticks; ++tick) {
        if (!std::holds_alternative<travel::Snapshot>(journey->step()))
            return false;
    }
    const auto exit = journey->snapshot();
    const travel::Command command{exit.vessel, exit.tick, 2};
    const auto arrive = [&]() -> Result<travel::Snapshot> {
        if (prefetch.selected() == proxy)
            return destination.arrive(*journey, command, exit.trip);
        const std::array obstacles{physics::Box{{1, 1, 1}, {5, 0, 0}, 1000, false}};
        auto staged = destination.publish({route.destination, route.collision_revision, obstacles});
        if (const auto *error = std::get_if<Error>(&staged))
            return *error;
        return destination.arrive(*journey, command, exit.trip);
    };
    const auto waiting = arrive();
    const auto *error = std::get_if<Error>(&waiting);
    if (exit.phase != travel::Phase::exit || !error || *error != Error::busy ||
        journey->snapshot().local || prefetch.selected() != proxy) {
        std::fputs("Crossing did not wait for GPU residency\n", stderr);
        return false;
    }
    auto visible_proxy = prefetch.selected();
    if (!probe::draw_scene(context, visible_proxy, {}))
        return false;
    if (files) {
        if (first_read->poll())
            return false;
        release_read.store(true);
        const auto retry = [](const std::filesystem::path &path) {
            tools::PackageReloader worker(path);
            if (!std::holds_alternative<std::monostate>(worker.request(1)))
                throw std::runtime_error("destination read was not admitted");
            return collect_destination(worker);
        };
        constexpr std::array issues{assets::Issue::io, assets::Issue::invalid};
        for (std::size_t i = 0; i < issues.size(); ++i) {
            const auto rejected = i == 0 ? collect_destination(*first_read) : retry((*files)[i]);
            const auto *failure = std::get_if<assets::Failure>(&rejected);
            const auto waiting_again = arrive();
            const auto *still_busy = std::get_if<Error>(&waiting_again);
            if (!failure || failure->issue != issues[i] || !still_busy ||
                *still_busy != Error::busy || journey->snapshot().local ||
                prefetch.selected() != proxy)
                return false;
        }
        auto ready = retry(files->back());
        auto *package = std::get_if<assets::PackagePin>(&ready);
        if (!package)
            return false;
        const auto baseline_result = context.resource_statistics();
        if (failed(baseline_result, "crossing pressure baseline"))
            return false;
        const auto baseline = std::get<render::ResourceStatistics>(baseline_result);
        if (baseline.retiring != 0 || baseline.live >= render::resource_record_limit)
            return false;
        {
            std::vector<render::Buffer> fillers;
            for (auto slots = baseline.live + baseline.retiring + 1;
                 slots < render::resource_record_limit; ++slots) {
                auto buffer = context.create_buffer({16, VK_BUFFER_USAGE_TRANSFER_SRC_BIT});
                if (failed(buffer, "crossing pressure allocation"))
                    return false;
                fillers.push_back(std::get<render::Buffer>(std::move(buffer)));
            }
            const auto pressure = prefetch.begin(context, *package);
            const auto blocked = arrive();
            const auto *busy = std::get_if<Error>(&blocked);
            if (!pressure || pressure->message != "resource slot capacity exhausted" || !busy ||
                *busy != Error::busy || prefetch.selected() != proxy || journey->snapshot().local) {
                std::fputs("Resource pressure did not preserve the crossing gate\n", stderr);
                return false;
            }
        }
        const auto reclaimed = context.resource_statistics();
        if (failed(reclaimed, "crossing pressure reclaim"))
            return false;
        const auto after = std::get<render::ResourceStatistics>(reclaimed);
        if (after.live != baseline.live || after.retiring != 0 ||
            after.allocation_bytes != baseline.allocation_bytes ||
            prefetch.begin(context, std::move(*package)))
            return false;
        std::puts("crossing_slot_pressure=1 partial_upload_reclaimed=1 proxy_preserved=1");
        std::puts("departure_prefetch_pending_at_exit=1 destination_missing_rejected=1 "
                  "destination_corrupt_rejected=1 file_retry=1");
    }
    if (sampled_bytes) {
        const auto advance = [&] {
            if (!sample() || prefetch.advance(context))
                throw std::runtime_error("crossing upload failed during queued frame probe");
            return prefetch.selected() == proxy;
        };
        if (!probe::draw_scene(context, visible_proxy, {}, {}, false, advance))
            return false;
    } else {
        const auto start = SDL_GetTicks();
        while (prefetch.selected() == proxy) {
            if (prefetch.advance(context) || SDL_GetTicks() - start >= 2000)
                return false;
            SDL_Delay(1);
        }
    }
    const auto completed = arrive();
    if (const auto *landed = std::get_if<travel::Snapshot>(&completed);
        landed && landed->local && landed->phase == travel::Phase::arrived &&
        landed->tick == exit.tick && landed->local->frame.origin == route.destination) {
        std::puts("crossing_waited_for_gpu=1 destination_resident=1 arrival_tick_preserved=1");
        return sample();
    }
    return false;
}

bool replay_crossing(ngen::render::Context &context, const ngen::render::ScenePin &proxy,
                     const std::optional<CrossingFiles> &files) {
    using Clock = std::chrono::steady_clock;
    if (!crossing(context, proxy, files))
        return false;
    const auto settle = [&] {
        const auto deadline = Clock::now() + std::chrono::seconds(2);
        do {
            const auto result = context.resource_statistics();
            if (failed(result, "crossing retirement"))
                break;
            const auto stats = std::get<ngen::render::ResourceStatistics>(result);
            if (stats.retiring == 0)
                return std::optional{stats};
            SDL_Delay(1);
        } while (Clock::now() < deadline);
        return std::optional<ngen::render::ResourceStatistics>{};
    };
    const auto baseline = settle();
    if (!baseline)
        return false;
    std::array<double, 20> samples{};
    std::uint64_t sampled_bytes = 0;
    for (auto &sample : samples) {
        const auto start = Clock::now();
        if (!crossing(context, proxy, files, &sampled_bytes))
            return false;
        sample = std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        const auto settled = settle();
        if (!settled || settled->live != baseline->live ||
            settled->allocation_bytes != baseline->allocation_bytes) {
            std::fputs("Crossing did not return to the settled resource baseline\n", stderr);
            return false;
        }
    }
    std::ranges::sort(samples);
    std::printf("warm_crossings=%zu crossing_ms_p50=%.3f crossing_ms_p95=%.3f "
                "crossing_ms_max=%.3f sampled_upload_allocation_bytes=%llu "
                "settled_bytes=%llu\n",
                samples.size(), (samples[9] + samples[10]) / 2, samples[18], samples.back(),
                static_cast<unsigned long long>(sampled_bytes),
                static_cast<unsigned long long>(baseline->allocation_bytes));
    return true;
}
#endif
ngen::assets::PackageResult fixture() {
    const auto texture_path = std::filesystem::path{NGEN_TEST_FIXTURES} / "rgba8-srgb.ktx2";
    const auto length = std::filesystem::file_size(texture_path);
    if (length > 4096)
        return ngen::assets::Failure{ngen::assets::Issue::capacity, "synthetic texture size"};
    std::vector<std::byte> texture(static_cast<std::size_t>(length));
    std::ifstream input(texture_path, std::ios::binary);
    input.read(reinterpret_cast<char *>(texture.data()),
               static_cast<std::streamsize>(texture.size()));
    if (!input)
        return ngen::assets::Failure{ngen::assets::Issue::io, "synthetic texture read"};
    const auto bytes = package_fixture::encode(package_fixture::textured_triangle(texture));
    return ngen::assets::load_package(bytes);
}
} // namespace
int main(int argc, char **argv) try {
#ifdef NGEN_SCENE_PROBE_PHYSICS
    std::optional<CrossingFiles> files;
    const bool replay = argc == 5 && std::string_view(argv[1]) == "--crossing-replay";
    if (argc == 5 && (replay || std::string_view(argv[1]) == "--crossing-files")) {
        files = CrossingFiles{argv[2], argv[3], argv[4]};
        argc = 1;
    }
#endif
    if (argc > 3) {
        std::fputs("Usage: scene_probe [scene.ngas [capture.bmp]]\n", stderr);
#ifdef NGEN_SCENE_PROBE_PHYSICS
        std::fputs("   or: scene_probe --crossing-files|--crossing-replay "
                   "missing.ngas corrupt.ngas valid.ngas\n",
                   stderr);
#endif
        return 2;
    }
    if (argc == 3 && (std::filesystem::exists(std::filesystem::symlink_status(argv[2])) ||
                      std::filesystem::path(argv[2]).extension() != ".bmp")) {
        std::fputs("Scene capture must be a new .bmp path; existing files/links are preserved\n",
                   stderr);
        return 2;
    }
    auto loaded = argc >= 2 ? ngen::assets::load_package_file(argv[1]) : fixture();
    if (const auto *failure = std::get_if<ngen::assets::Failure>(&loaded)) {
        std::fprintf(stderr, "Scene fixture: %s\n", failure->message.c_str());
        return 1;
    }
    auto cpu = std::get<ngen::assets::PackagePin>(loaded);
    auto creation = ngen::render::Context::create({true, false, true});
    if (failed(creation, "context"))
        return 1;
    auto context = std::get<std::unique_ptr<ngen::render::Context>>(std::move(creation));
    auto begun = ngen::render::SceneUpload::begin(*context, cpu);
    if (failed(begun, "begin checked scene upload"))
        return 1;
    cpu.reset();
    loaded = ngen::assets::Failure{ngen::assets::Issue::cancelled, "caller storage released"};
    auto upload = std::get<std::unique_ptr<ngen::render::SceneUpload>>(std::move(begun));
    const auto completed = finish(*context, *upload);
    if (failed(completed, "advance upload"))
        return 1;
    const auto version = std::get<ngen::render::ScenePin>(completed);
    upload.reset();
    if (!cancel_inflight(*context, version))
        return 1;
#ifdef NGEN_SCENE_PROBE_PHYSICS
    if (argc == 1 && !(replay ? replay_crossing(*context, version, files)
                              : crossing(*context, version, files))) {
        std::fputs("Two-system crossing failed\n", stderr);
        return 1;
    }
#endif
    {
        ngen::render::ScenePrefetch prefetch(version);
        if (prefetch.begin(*context, version->package()) || prefetch.advance(*context) ||
            prefetch.selected() != version || !prefetch.begin(*context, version->package())) {
            std::fputs("Prefetch did not retain its proxy during a bounded upload\n", stderr);
            return 1;
        }
        std::stop_source stop_prefetch;
        (void)stop_prefetch.request_stop();
        if (!prefetch.advance(*context, stop_prefetch.get_token()) ||
            prefetch.selected() != version || prefetch.begin(*context, version->package())) {
            std::fputs("Cancelled prefetch changed its proxy or prevented retry\n", stderr);
            return 1;
        }
        const auto prefetch_start = SDL_GetTicks();
        while (prefetch.selected() == version) {
            if (prefetch.advance(*context) || SDL_GetTicks() - prefetch_start >= 2000) {
                std::fputs("Prefetch failed to publish its completed scene\n", stderr);
                return 1;
            }
            SDL_Delay(1);
        }
        auto frame_pin = prefetch.selected();
        const std::weak_ptr<const ngen::render::SceneVersion> retired = frame_pin;
        if (prefetch.begin(*context, version->package()))
            return 1;
        prefetch.evict();
        if (prefetch.selected() != version || retired.expired() ||
            !frame_pin->vertices().native()) {
            std::fputs("Eviction lost the proxy or invalidated a retained version\n", stderr);
            return 1;
        }
        frame_pin.reset();
        if (!retired.expired() || prefetch.begin(*context, version->package())) {
            std::fputs("Eviction kept a cache pin or left a pending upload\n", stderr);
            return 1;
        }
        std::puts("destination_eviction=1 proxy_restored=1 retained_version_valid=1 retry=1");
    }
    auto replacement_created = ngen::render::SceneUpload::begin(*context, version->package());
    if (failed(replacement_created, "replacement while active version is pinned"))
        return 1;
    auto replacement =
        std::get<std::unique_ptr<ngen::render::SceneUpload>>(std::move(replacement_created));
    std::stop_source cancelled;
    (void)cancelled.request_stop();
    const auto rejected = replacement->advance(*context, cancelled.get_token());
    if (!std::holds_alternative<ngen::render::Failure>(rejected) || !version->vertices().native()) {
        std::fputs("Cancelled replacement changed the active immutable version\n", stderr);
        return 1;
    }
    replacement.reset();
    auto next_created = ngen::render::SceneUpload::begin(*context, version->package());
    if (failed(next_created, "second pinned version"))
        return 1;
    auto next_upload =
        std::get<std::unique_ptr<ngen::render::SceneUpload>>(std::move(next_created));
    auto next_completed = finish(*context, *next_upload);
    if (failed(next_completed, "replacement publication"))
        return 1;
    auto next = std::get<ngen::render::ScenePin>(std::move(next_completed));
    next_upload.reset();
    if (next == version || next->vertices().native() == version->vertices().native() ||
        !version->vertices().native()) {
        std::fputs("Pinned versions aliased native geometry\n", stderr);
        return 1;
    }
    const auto resident_statistics = context->resource_statistics();
    if (failed(resident_statistics, "two-version statistics"))
        return 1;
    std::printf(
        "pinned_versions=2 resident_allocation_bytes=%llu resident_reserved_bytes=%llu\n",
        static_cast<unsigned long long>(
            std::get<ngen::render::ResourceStatistics>(resident_statistics).allocation_bytes),
        static_cast<unsigned long long>(
            std::get<ngen::render::ResourceStatistics>(resident_statistics).reserved_bytes));
    const auto start = SDL_GetTicks();
    if (version->package()->vertices.empty() || version->package()->instances.empty() ||
        !version->vertices().native() || !version->indices().native()) {
        std::fputs("Published scene lost owning geometry/version data\n", stderr);
        return 1;
    }
    const auto expected = std::as_bytes(std::span(version->package()->vertices));
    VkDeviceSize readback_size = std::max<VkDeviceSize>(
        expected.size(), std::as_bytes(std::span(version->package()->indices)).size());
    for (const auto &image : version->package()->images) {
        VkDeviceSize bytes = 0;
        for (const auto &mip : image.mips)
            bytes += mip.pixels.size();
        readback_size = std::max(readback_size, bytes);
    }
    auto allocated = context->create_buffer(
        {readback_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, ngen::render::MemoryAccess::readback});
    if (failed(allocated, "readback buffer"))
        return 1;
    auto readback = std::get<ngen::render::Buffer>(std::move(allocated));
    const auto copied = context->copy_buffer({version->vertices(), readback, expected.size()});
    if (failed(copied, "resident vertex readback"))
        return 1;
    for (;;) {
        const auto complete =
            context->submission_complete(std::get<ngen::render::Submission>(copied));
        if (failed(complete, "resident completion"))
            return 1;
        if (std::get<bool>(complete))
            break;
        if (SDL_GetTicks() - start >= 5000)
            return 1;
        SDL_Delay(1);
    }
    std::vector<std::byte> actual(expected.size());
    if (failed(readback.read(0, actual), "resident vertex bytes") ||
        !std::equal(actual.begin(), actual.end(), expected.begin(), expected.end())) {
        std::fputs("Resident geometry GPU bytes differ\n", stderr);
        return 1;
    }
    const auto expected_indices = std::as_bytes(std::span(version->package()->indices));
    const auto index_copy =
        context->copy_buffer({version->indices(), readback, expected_indices.size()});
    if (failed(index_copy, "resident index readback"))
        return 1;
    for (;;) {
        const auto complete =
            context->submission_complete(std::get<ngen::render::Submission>(index_copy));
        if (failed(complete, "resident index completion"))
            return 1;
        if (std::get<bool>(complete))
            break;
        if (SDL_GetTicks() - start >= 5000)
            return 1;
        SDL_Delay(1);
    }
    std::vector<std::byte> actual_indices(expected_indices.size());
    if (failed(readback.read(0, actual_indices), "resident index bytes") ||
        !std::equal(actual_indices.begin(), actual_indices.end(), expected_indices.begin(),
                    expected_indices.end())) {
        std::fputs("Resident GPU indices differ\n", stderr);
        return 1;
    }
    if (version->images().size() != version->package()->images.size() ||
        version->images().empty()) {
        std::fputs("Resident scene lost its image\n", stderr);
        return 1;
    }
    std::uint64_t verified_mip_bytes = 0;
    for (std::size_t image = 0; image < version->images().size(); ++image) {
        const auto image_copy = context->copy_from_image({version->images()[image], readback});
        if (failed(image_copy, "resident image readback"))
            return 1;
        for (;;) {
            const auto complete =
                context->submission_complete(std::get<ngen::render::Submission>(image_copy));
            if (failed(complete, "resident mip completion"))
                return 1;
            if (std::get<bool>(complete))
                break;
            if (SDL_GetTicks() - start >= 5000)
                return 1;
            SDL_Delay(1);
        }
        std::vector<std::byte> expected_mips;
        for (const auto &mip : version->package()->images[image].mips)
            expected_mips.insert(expected_mips.end(), mip.pixels.begin(), mip.pixels.end());
        std::vector<std::byte> actual_mips(expected_mips.size());
        if (failed(readback.read(0, actual_mips), "resident image pixels") ||
            actual_mips != expected_mips) {
            std::fputs("Resident mip bytes differ\n", stderr);
            return 1;
        }
        verified_mip_bytes += expected_mips.size();
    }
    std::printf("resident_verified_mip_bytes=%llu\n",
                static_cast<unsigned long long>(verified_mip_bytes));
    std::fflush(stdout);
    if (!ngen::probe::draw_scene(*context, next, version, argc == 3 ? argv[2] : "", argc == 1))
        return 1;
    if (!context->close() || version->vertices().native() || version->indices().native() ||
        version->images()[0].native())
        return 1;
    std::puts("checked_scene_upload=1 immutable_version=1 owning_cpu_pin=1 "
              "resident_vertex_readback=1 resident_index_readback=1 resident_mip_readback=1 "
              "cancelled_replacement=1 "
              "close_invalidated=1");
    std::printf(
        "resident_vertices=%zu resident_indices=%zu resident_images=%zu retained_lights=%zu\n",
        version->package()->vertices.size(), version->package()->indices.size(),
        version->package()->images.size(), version->package()->lights.size());
    std::array<std::size_t, 3> kinds{};
    for (const auto &light : version->package()->lights)
        ++kinds[light.type];
    std::size_t uv1_maps = 0;
    for (const auto &material : version->package()->materials)
        for (const auto &map : material.maps)
            if (map.texture != UINT32_MAX && map.uv_set == 1)
                ++uv1_maps;
    std::printf("source_directional=%zu source_point=%zu source_spot=%zu source_uv1_maps=%zu\n",
                kinds[0], kinds[1], kinds[2], uv1_maps);
    std::array<std::size_t, 3> alpha{};
    std::size_t two_sided = 0;
    for (const auto &material : version->package()->materials) {
        ++alpha[material.alpha];
        two_sided += material.double_sided ? 1U : 0U;
    }
    std::printf("source_opaque=%zu source_mask=%zu source_blend=%zu source_double_sided=%zu\n",
                alpha[0], alpha[1], alpha[2], two_sided);
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "Scene probe: %s\n", error.what());
    return 1;
}
