#include "flight_demo/performance.hpp"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <semaphore>
#include <stdexcept>
#include <string_view>
#include <thread>

TEST_CASE("unavailable or regressed CPU samples never fabricate a busy percentage") {
    ngen::demo::SystemUsageHistory history;
    (void)history.advance({ngen::demo::CpuCounters{100, 1000}, {}, {}});
    REQUIRE_FALSE(history.advance({ngen::demo::CpuCounters{500, 1100}, {}, {}}).cpu_percent);
    REQUIRE(history.advance({ngen::demo::CpuCounters{500, 1200}, {}, {}}).cpu_percent == 100);
    REQUIRE_FALSE(history.advance({}).cpu_percent);
    REQUIRE_FALSE(history.advance({ngen::demo::CpuCounters{510, 1300}, {}, {}}).cpu_percent);
    REQUIRE_FALSE(history.advance({ngen::demo::CpuCounters{1400, 1300}, {}, {}}).cpu_percent);
}

TEST_CASE("OS telemetry exceptions publish unavailable values instead of escaping the worker") {
    using namespace std::chrono_literals;
    ngen::demo::SystemUsageSampler sampler{{}, []() -> ngen::demo::SystemUsageReading {
                                               throw std::runtime_error("OS reader unavailable");
                                           }};
    sampler.enabled(true);
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!sampler.snapshot().sequence && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    sampler.enabled(false);
    const auto sample = sampler.snapshot();
    REQUIRE(sample.sequence > 0);
    REQUIRE(sample.collection_failed);
    REQUIRE_FALSE(sample.values.cpu_percent);
    REQUIRE_FALSE(sample.values.ram);
    REQUIRE_FALSE(sample.values.vram);
}

TEST_CASE("whole-device VRAM counters bind to the selected adapter rather than a process or "
          "default GPU") {
    ngen::demo::GpuMemoryIdentity gpu;
    gpu.kind = ngen::demo::GpuMemoryKind::dedicated;
    gpu.total_bytes = 12ULL * 1024 * 1024 * 1024;
    gpu.luid = {0x78, 0x56, 0x34, 0x12, 0xef, 0xcd, 0xab, 0x90};
    gpu.luid_valid = true;
    gpu.node_mask = 1;
    const auto path = ngen::demo::gpu_memory_counter_path(gpu);
    REQUIRE(path.has_value());
    const auto text = path.value_or(std::array<wchar_t, 160>{});
    REQUIRE(std::wstring_view(text.data()) ==
            LR"(\GPU Adapter Memory(luid_0x90abcdef_0x12345678_phys_0)\Dedicated Usage)");
    gpu.kind = ngen::demo::GpuMemoryKind::shared;
    REQUIRE_FALSE(ngen::demo::gpu_memory_counter_path(gpu));
    gpu.kind = ngen::demo::GpuMemoryKind::dedicated;
    gpu.luid_valid = false;
    REQUIRE_FALSE(ngen::demo::gpu_memory_counter_path(gpu));
    gpu.luid_valid = true;
    gpu.node_mask = 3;
    REQUIRE_FALSE(ngen::demo::gpu_memory_counter_path(gpu));
}

TEST_CASE("disabled samplers stop without polling while the worker enters its wait") {
    std::atomic<unsigned> calls{};
    for (unsigned i = 0; i < 500; ++i) {
        ngen::demo::SystemUsageSampler sampler{{}, [&] {
                                                   ++calls;
                                                   return ngen::demo::SystemUsageReading{};
                                               }};
        REQUIRE(sampler.snapshot().sequence == 0);
        std::this_thread::yield();
    }
    REQUIRE(calls.load() == 0);
}

TEST_CASE("native system samples provide whole-system RAM and CPU without a graphics context") {
    using namespace std::chrono_literals;
    ngen::demo::GpuMemoryIdentity gpu;
    gpu.kind = ngen::demo::GpuMemoryKind::shared;
    ngen::demo::SystemUsageSampler sampler{gpu};
    sampler.enabled(true);
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (sampler.snapshot().sequence < 2 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    sampler.enabled(false);
    const auto sample = sampler.snapshot();
    REQUIRE(sample.sequence >= 2);
    REQUIRE(sample.values.ram.has_value());
    const auto ram = sample.values.ram.value_or(ngen::demo::MemoryBytes{});
    REQUIRE(ram.total > 0);
    REQUIRE(ram.used <= ram.total);
    REQUIRE(sample.values.cpu_percent.has_value());
    const auto cpu = sample.values.cpu_percent.value_or(-1);
    REQUIRE(std::isfinite(cpu));
    REQUIRE(cpu >= 0);
    REQUIRE(cpu <= 100);
    REQUIRE_FALSE(sample.values.vram);
}

TEST_CASE("system sampling runs off the caller and pauses while the overlay is hidden") {
    using namespace std::chrono_literals;
    std::binary_semaphore entered{0};
    std::counting_semaphore<4> release{0};
    std::atomic<unsigned> reads{};
    ngen::demo::SystemUsageSampler sampler{
        {}, [&] {
            ++reads;
            entered.release();
            release.acquire();
            return ngen::demo::SystemUsageReading{
                ngen::demo::CpuCounters{10, 100}, ngen::demo::MemoryBytes{4, 16}, {}};
        }};
    struct ReleaseOnExit {
        std::counting_semaphore<4> &semaphore;
        ~ReleaseOnExit() { semaphore.release(); }
    } cleanup{release};
    REQUIRE(sampler.snapshot().sequence == 0);
    REQUIRE(reads.load() == 0);
    sampler.enabled(true);
    REQUIRE(entered.try_acquire_for(2s));
    REQUIRE(sampler.snapshot().sequence == 0);
    sampler.enabled(false);
    release.release();
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (sampler.snapshot().sequence == 0 && std::chrono::steady_clock::now() < deadline)
        std::this_thread::yield();
    const auto sample = sampler.snapshot();
    REQUIRE(sample.sequence == 1);
    REQUIRE(sample.values.ram.has_value());
    REQUIRE(sample.values.ram.value_or(ngen::demo::MemoryBytes{}).used == 4);
    REQUIRE_FALSE(entered.try_acquire_for(1200ms));
    REQUIRE(reads.load() == 1);
}

TEST_CASE(
    "system memory readings preserve valid zero and reject unavailable or impossible totals") {
    ngen::demo::SystemUsageHistory history;
    const auto sample =
        history.advance({{}, ngen::demo::MemoryBytes{8, 16}, ngen::demo::MemoryBytes{0, 12}});
    REQUIRE(sample.ram.has_value());
    const auto ram = sample.ram.value_or(ngen::demo::MemoryBytes{});
    REQUIRE(ram.used == 8);
    REQUIRE(ram.total == 16);
    REQUIRE(sample.vram.has_value());
    const auto vram = sample.vram.value_or(ngen::demo::MemoryBytes{});
    REQUIRE(vram.used == 0);
    REQUIRE(vram.total == 12);
    const auto invalid =
        history.advance({{}, ngen::demo::MemoryBytes{17, 16}, ngen::demo::MemoryBytes{0, 0}});
    REQUIRE_FALSE(invalid.ram);
    REQUIRE_FALSE(invalid.vram);
    const auto missing = history.advance({});
    REQUIRE_FALSE(missing.ram);
    REQUIRE_FALSE(missing.vram);
}

TEST_CASE("whole-system CPU usage measures busy time between successive OS samples") {
    ngen::demo::SystemUsageHistory history;
    const auto first = history.advance({ngen::demo::CpuCounters{100, 1000}, {}, {}});
    REQUIRE_FALSE(first.cpu_percent);
    const auto next = history.advance({ngen::demo::CpuCounters{125, 1100}, {}, {}});
    REQUIRE(next.cpu_percent.has_value());
    REQUIRE(std::abs(next.cpu_percent.value_or(-1) - 75) < 1e-9);
    const auto busy = history.advance({ngen::demo::CpuCounters{125, 1200}, {}, {}});
    REQUIRE(busy.cpu_percent.has_value());
    REQUIRE(busy.cpu_percent.value_or(-1) == 100);
}

TEST_CASE("only fresh F12 presses from the flight window toggle the hidden performance overlay") {
    ngen::demo::PerformanceControls controls{7};
    REQUIRE_FALSE(controls.visible());
    SDL_Event event{};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = 7;
    event.key.scancode = SDL_SCANCODE_F12;
    REQUIRE(controls.event(event));
    REQUIRE(controls.visible());
    event.key.repeat = true;
    REQUIRE_FALSE(controls.event(event));
    REQUIRE(controls.visible());
    event.key.repeat = false;
    event.type = SDL_EVENT_KEY_UP;
    REQUIRE_FALSE(controls.event(event));
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = 8;
    REQUIRE_FALSE(controls.event(event));
    event.key.windowID = 7;
    event.key.scancode = SDL_SCANCODE_F1;
    REQUIRE_FALSE(controls.event(event));
    REQUIRE(controls.visible());
    event.key.scancode = SDL_SCANCODE_F12;
    REQUIRE(controls.event(event));
    REQUIRE_FALSE(controls.visible());
}
