#pragma once
#include <SDL3/SDL_events.h>
#include <array>
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>

namespace ngen::demo {
struct CpuCounters {
    std::uint64_t idle{}, total{};
};
struct MemoryBytes {
    std::uint64_t used{}, total{};
};
struct SystemUsageReading {
    std::optional<CpuCounters> cpu;
    std::optional<MemoryBytes> ram, vram;
};
struct SystemUsageSnapshot {
    std::optional<double> cpu_percent;
    std::optional<MemoryBytes> ram, vram;
};
class SystemUsageHistory {
  public:
    [[nodiscard]] SystemUsageSnapshot advance(const SystemUsageReading &reading) noexcept;

  private:
    std::optional<CpuCounters> previous_cpu_;
};
enum class GpuMemoryKind : std::uint8_t { unavailable, shared, dedicated };
struct GpuMemoryIdentity {
    std::uint64_t total_bytes{};
    std::array<std::uint8_t, 8> luid{};
    std::uint32_t node_mask{};
    GpuMemoryKind kind{GpuMemoryKind::unavailable};
    bool luid_valid{};
};
// Exact selected-adapter PDH path; unavailable/shared/multi-node identities are not guessed.
[[nodiscard]] std::optional<std::array<wchar_t, 160>>
gpu_memory_counter_path(const GpuMemoryIdentity &gpu) noexcept;
class SystemUsageSampler {
  public:
    struct Sample {
        SystemUsageSnapshot values;
        std::chrono::steady_clock::time_point sampled_at{};
        std::uint64_t sequence{};
        bool collection_failed{};
    };
    using Reader = std::function<SystemUsageReading()>;
    // Reader is the OS boundary; its owned captures must not retain gameplay or GPU handles.
    explicit SystemUsageSampler(GpuMemoryIdentity gpu, Reader read = {});
    ~SystemUsageSampler();
    SystemUsageSampler(const SystemUsageSampler &) = delete;
    SystemUsageSampler &operator=(const SystemUsageSampler &) = delete;
    void enabled(bool value);
    [[nodiscard]] Sample snapshot() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
class PerformanceControls {
  public:
    explicit PerformanceControls(SDL_WindowID window) : window_(window) {}
    // Only a fresh F12 key-down from this window changes the persistent preference.
    [[nodiscard]] bool event(const SDL_Event &event) noexcept;
    [[nodiscard]] bool visible() const noexcept { return visible_; }

  private:
    SDL_WindowID window_{};
    bool visible_{};
};
} // namespace ngen::demo
