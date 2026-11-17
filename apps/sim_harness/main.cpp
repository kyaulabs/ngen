#include "simulation/simulation.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string_view>

namespace {
std::atomic<std::uint64_t> allocations{};
#if NGEN_ALLOCATION_PROBE
void *allocate(std::size_t size) {
    ++allocations;
    if (void *ptr = std::malloc(size ? size : 1))
        return ptr;
    throw std::bad_alloc{};
}
#endif
} // namespace
#if NGEN_ALLOCATION_PROBE
void *operator new(std::size_t size) { return allocate(size); }
void *operator new[](std::size_t size) { return allocate(size); }
void operator delete(void *ptr) noexcept { std::free(ptr); }
void operator delete[](void *ptr) noexcept { std::free(ptr); }
void operator delete(void *ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void *ptr, std::size_t) noexcept { std::free(ptr); }
#endif

int main(int argc, char **argv) try {
    unsigned workers = 0;
    if (argc == 2 && std::string_view(argv[1]) == "--workers=2")
        workers = 2;
    else if (argc != 1) {
        std::fputs("Usage: sim_harness [--workers=2]\n", stderr);
        return 2;
    }
    ngen::Simulation sim(workers);
    std::array<ngen::Entity, 8> entities{};
    for (auto &entity : entities)
        entity = std::get<ngen::Entity>(sim.create());
    (void)std::get<ngen::Snapshot>(sim.step()); // warm scheduler and publication
    const auto initial = allocations.load();
    const auto enki_initial = ngen::Jobs::allocation_count();
    std::array<std::int64_t, 1000> timings{};
    std::uint32_t random = 0x4e47454e;
    std::uint64_t sequence = 0;
    for (std::size_t tick = 0; tick < timings.size(); ++tick) {
        const auto start = std::chrono::steady_clock::now();
        for (std::size_t command = 0; command < 32; ++command) {
            random = random * 1664525U + 1013904223U;
            const auto displacement = static_cast<std::int64_t>(random % 201) - 100;
            const auto result = sim.submit({ngen::Tick{tick + 2},
                                            ++sequence,
                                            entities[command % entities.size()],
                                            {displacement}});
            if (std::holds_alternative<ngen::Error>(result))
                return 3;
        }
        if (std::holds_alternative<ngen::Error>(sim.step()))
            return 4;
        timings[tick] = std::chrono::duration_cast<std::chrono::nanoseconds>(
                            std::chrono::steady_clock::now() - start)
                            .count();
    }
    const auto heap_delta = allocations.load() - initial;
    const auto scheduler_delta = ngen::Jobs::allocation_count() - enki_initial;
    const auto snapshot = sim.snapshot();
    constexpr std::array<std::int64_t, 8> expected{7303,  4415, -5166, 425,
                                                   -3267, 2214, 9433,  -3427};
    std::int64_t sum = 0;
    for (std::size_t i = 0; i < entities.size(); ++i) {
        if (snapshot.poses()[i].position.value != expected[i])
            return 5;
        sum += snapshot.poses()[i].position.value;
    }
    std::sort(timings.begin(), timings.end());
    std::printf("seed=0x4e47454e workers=%u ticks=1000 entities=8 commands=32000 sum=%lld\n"
                "tick_ns p50=%lld p95=%lld p99=%lld allocation_probe=%d new_allocations=%llu "
                "scheduler_allocations=%llu\n",
                workers, static_cast<long long>(sum), static_cast<long long>(timings[500]),
                static_cast<long long>(timings[950]), static_cast<long long>(timings[990]),
                NGEN_ALLOCATION_PROBE, static_cast<unsigned long long>(heap_delta),
                static_cast<unsigned long long>(scheduler_delta));
    return heap_delta || scheduler_delta ? 6 : 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "CPU harness: %s\n", error.what());
    return 1;
}
