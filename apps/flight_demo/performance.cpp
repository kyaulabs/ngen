#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif
#include "flight_demo/performance.hpp"
#include <charconv>
#include <condition_variable>
#include <cwchar>
#include <limits>
#include <mutex>
#include <span>
#include <string_view>
#include <thread>
#include <utility>
#if defined(_WIN32)
#include <pdh.h>
#include <pdhmsg.h>
#include <windows.h>
#elif defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ngen::demo {
namespace {
#if defined(__linux__)
std::size_t read_proc(const char *path, std::span<char> storage) {
    const auto file = ::open(path, O_RDONLY | O_CLOEXEC);
    if (file < 0)
        return 0;
    const auto count = ::read(file, storage.data(), storage.size());
    (void)::close(file);
    return count > 0 ? static_cast<std::size_t>(count) : 0;
}
std::optional<std::uint64_t> counter(std::string_view &text) {
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string_view::npos)
        return {};
    text.remove_prefix(first);
    std::uint64_t value{};
    const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
    if (parsed.ec != std::errc{} ||
        (parsed.ptr != text.data() + text.size() && *parsed.ptr != ' ' && *parsed.ptr != '\t'))
        return {};
    text.remove_prefix(static_cast<std::size_t>(parsed.ptr - text.data()));
    return value;
}
#endif
SystemUsageReading read_system_usage() {
    SystemUsageReading result;
#if defined(_WIN32)
    FILETIME idle{}, kernel{}, user{};
    if (GetActiveProcessorGroupCount() == 1 && GetSystemTimes(&idle, &kernel, &user)) {
        const auto ticks = [](FILETIME value) {
            return (static_cast<std::uint64_t>(value.dwHighDateTime) << 32U) | value.dwLowDateTime;
        };
        const auto kernel_ticks = ticks(kernel), user_ticks = ticks(user);
        if (kernel_ticks <= std::numeric_limits<std::uint64_t>::max() - user_ticks)
            result.cpu = CpuCounters{ticks(idle), kernel_ticks + user_ticks};
    }
    MEMORYSTATUSEX memory{};
    memory.dwLength = static_cast<DWORD>(sizeof(memory));
    if (GlobalMemoryStatusEx(&memory) && memory.ullAvailPhys <= memory.ullTotalPhys)
        result.ram = MemoryBytes{memory.ullTotalPhys - memory.ullAvailPhys, memory.ullTotalPhys};
#elif defined(__linux__)
    std::array<char, 1024> cpu_storage{};
    auto cpu = std::string_view(cpu_storage.data(), read_proc("/proc/stat", cpu_storage));
    cpu = cpu.substr(0, cpu.find('\n'));
    if (cpu.starts_with("cpu ")) {
        cpu.remove_prefix(4);
        std::array<std::uint64_t, 8> ticks{};
        bool valid = true;
        std::uint64_t total{};
        for (auto &tick : ticks) {
            const auto parsed = counter(cpu);
            if (!parsed || total > std::numeric_limits<std::uint64_t>::max() - *parsed) {
                valid = false;
                break;
            }
            tick = *parsed;
            total += tick;
        }
        if (valid)
            result.cpu = CpuCounters{ticks[3] + ticks[4], total};
    }
    std::array<char, 65536> memory_storage{};
    const auto memory =
        std::string_view(memory_storage.data(), read_proc("/proc/meminfo", memory_storage));
    const auto kibibytes = [&](std::string_view name) -> std::optional<std::uint64_t> {
        auto remaining = memory;
        while (!remaining.empty()) {
            const auto end = remaining.find('\n');
            auto line = remaining.substr(0, end);
            if (line.starts_with(name)) {
                line.remove_prefix(name.size());
                const auto value = counter(line);
                const auto units = line.find_first_not_of(" \t");
                if (value && units != std::string_view::npos && line.substr(units) == "kB" &&
                    *value <= std::numeric_limits<std::uint64_t>::max() / 1024)
                    return *value * 1024;
                return {};
            }
            if (end == std::string_view::npos)
                break;
            remaining.remove_prefix(end + 1);
        }
        return {};
    };
    const auto total = kibibytes("MemTotal:"), available = kibibytes("MemAvailable:");
    if (total && available && *available <= *total)
        result.ram = MemoryBytes{*total - *available, *total};
#endif
    return result;
}
struct NativeUsageReader {
    GpuMemoryIdentity gpu;
#if defined(_WIN32)
    std::optional<std::array<wchar_t, 160>> path;
    PDH_HQUERY query{};
    PDH_HCOUNTER counter{};
#endif
    explicit NativeUsageReader(GpuMemoryIdentity identity) : gpu(identity) {
#if defined(_WIN32)
        path = gpu_memory_counter_path(gpu);
#endif
    }
    ~NativeUsageReader() {
#if defined(_WIN32)
        if (query)
            (void)PdhCloseQuery(query);
#endif
    }
    NativeUsageReader(const NativeUsageReader &) = delete;
    NativeUsageReader &operator=(const NativeUsageReader &) = delete;
    SystemUsageReading read() {
        auto result = read_system_usage();
#if defined(_WIN32)
        if (!path)
            return result;
        if (!query) {
            PDH_HQUERY next{};
            PDH_HCOUNTER selected{};
            if (PdhOpenQueryW(nullptr, 0, &next) != ERROR_SUCCESS)
                return result;
            if (PdhAddEnglishCounterW(next, path->data(), 0, &selected) != ERROR_SUCCESS) {
                (void)PdhCloseQuery(next);
                return result;
            }
            query = next;
            counter = selected;
        }
        PDH_FMT_COUNTERVALUE value{};
        if (PdhCollectQueryData(query) == ERROR_SUCCESS &&
            PdhGetFormattedCounterValue(counter, PDH_FMT_LARGE, nullptr, &value) == ERROR_SUCCESS &&
            (value.CStatus == PDH_CSTATUS_VALID_DATA || value.CStatus == PDH_CSTATUS_NEW_DATA) &&
            value.largeValue >= 0 &&
            static_cast<std::uint64_t>(value.largeValue) <= gpu.total_bytes)
            result.vram =
                MemoryBytes{static_cast<std::uint64_t>(value.largeValue), gpu.total_bytes};
#endif
        return result;
    }
};
} // namespace
SystemUsageSnapshot SystemUsageHistory::advance(const SystemUsageReading &reading) noexcept {
    SystemUsageSnapshot result;
    if (reading.cpu && reading.cpu->idle <= reading.cpu->total) {
        if (previous_cpu_ && reading.cpu->total > previous_cpu_->total &&
            reading.cpu->idle >= previous_cpu_->idle) {
            const auto total = reading.cpu->total - previous_cpu_->total;
            const auto idle = reading.cpu->idle - previous_cpu_->idle;
            if (idle <= total)
                result.cpu_percent =
                    100 * (1 - static_cast<double>(idle) / static_cast<double>(total));
        }
        previous_cpu_ = reading.cpu;
    } else
        previous_cpu_.reset();
    const auto admitted = [](const std::optional<MemoryBytes> &memory) {
        return memory && memory->total && memory->used <= memory->total;
    };
    if (admitted(reading.ram))
        result.ram = reading.ram;
    if (admitted(reading.vram))
        result.vram = reading.vram;
    return result;
}
std::optional<std::array<wchar_t, 160>>
gpu_memory_counter_path(const GpuMemoryIdentity &gpu) noexcept {
    if (gpu.kind != GpuMemoryKind::dedicated || !gpu.total_bytes || !gpu.luid_valid ||
        gpu.node_mask != 1)
        return {};
    const auto word = [&](std::size_t offset) {
        std::uint32_t value{};
        for (std::size_t i = 0; i < 4; ++i)
            value |= static_cast<std::uint32_t>(gpu.luid[offset + i]) << (i * 8);
        return static_cast<unsigned>(value);
    };
    std::array<wchar_t, 160> path{};
    const auto count = std::swprintf(
        path.data(), path.size(),
        LR"(\GPU Adapter Memory(luid_0x%08x_0x%08x_phys_0)\Dedicated Usage)", word(4), word(0));
    if (count <= 0 || static_cast<std::size_t>(count) >= path.size())
        return {};
    return path;
}
struct SystemUsageSampler::Impl {
    mutable std::mutex mutex;
    std::condition_variable changed;
    Sample sample;
    NativeUsageReader native;
    Reader read;
    bool enabled{};
    std::uint64_t epoch{};
    // Declared last: join before the captured state, condition variable and mutex die.
    std::jthread worker;
    Impl(GpuMemoryIdentity gpu, Reader source) : native(gpu), read(std::move(source)) {
        if (!read)
            read = [this] { return native.read(); };
        try {
            worker = std::jthread([this](std::stop_token stop) {
                SystemUsageHistory history;
                std::uint64_t sampled_epoch{};
                std::unique_lock lock(mutex);
                while (!stop.stop_requested()) {
                    changed.wait(lock, [&] { return stop.stop_requested() || enabled; });
                    if (stop.stop_requested())
                        break;
                    const auto requested_epoch = epoch;
                    lock.unlock();
                    if (sampled_epoch != requested_epoch) {
                        history = {};
                        sampled_epoch = requested_epoch;
                    }
                    const auto began = std::chrono::steady_clock::now();
                    SystemUsageReading reading;
                    bool failed = false;
                    try {
                        reading = read();
                    } catch (...) {
                        failed = true;
                    }
                    const auto values = history.advance(reading);
                    lock.lock();
                    sample = {values, began, sample.sequence + 1, failed};
                    changed.wait_for(lock, std::chrono::seconds(1), [&] {
                        return stop.stop_requested() || !enabled || epoch != requested_epoch;
                    });
                }
            });
        } catch (const std::system_error &) {
            sample.collection_failed = true;
        }
    }
    ~Impl() {
        {
            const std::lock_guard lock(mutex);
            worker.request_stop();
        }
        changed.notify_all();
    }
};
SystemUsageSampler::SystemUsageSampler(GpuMemoryIdentity gpu, Reader read)
    : impl_(std::make_unique<Impl>(gpu, std::move(read))) {}
SystemUsageSampler::~SystemUsageSampler() = default;
void SystemUsageSampler::enabled(bool value) {
    {
        const std::lock_guard lock(impl_->mutex);
        if (impl_->enabled == value)
            return;
        impl_->enabled = value;
        ++impl_->epoch;
    }
    impl_->changed.notify_all();
}
SystemUsageSampler::Sample SystemUsageSampler::snapshot() const {
    const std::lock_guard lock(impl_->mutex);
    return impl_->sample;
}
bool PerformanceControls::event(const SDL_Event &event) noexcept {
    if (!window_ || event.type != SDL_EVENT_KEY_DOWN || event.key.windowID != window_ ||
        event.key.scancode != SDL_SCANCODE_F12 || event.key.repeat)
        return false;
    visible_ = !visible_;
    return true;
}
} // namespace ngen::demo
