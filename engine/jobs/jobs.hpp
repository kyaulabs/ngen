#pragma once
#include "core/values.hpp"
#include <array>
#include <atomic>
#include <cstddef>
#include <enkiTS/TaskScheduler.h>

namespace ngen {
// Owner-thread API except request_cancel(). Callbacks borrow their context until
// drain()/shutdown() returns, must be finite, noexcept and never call this API.
class Jobs {
  public:
    static constexpr std::size_t capacity = 64;
    static constexpr std::size_t no_dependency = capacity;
    using Function = void (*)(void *, const std::atomic<bool> &) noexcept;
    explicit Jobs(unsigned workers); // 0 = serial; 1..7 worker threads
    ~Jobs();
    Jobs(const Jobs &) = delete;
    Jobs &operator=(const Jobs &) = delete;
    [[nodiscard]] Result<std::size_t> submit(Function, void *, std::size_t after = no_dependency);
    [[nodiscard]] Result<std::monostate> start();
    void request_cancel() noexcept;
    void drain();
    void shutdown();
    [[nodiscard]] static std::uint64_t allocation_count() noexcept;

  private:
    struct Task final : enki::ITaskSet {
        Function function{};
        void *context{};
        const std::atomic<bool> *cancelled{};
        std::size_t after{no_dependency};
        void ExecuteRange(enki::TaskSetPartition, std::uint32_t) override;
    };
    enki::TaskScheduler scheduler_;
    std::array<Task, capacity> tasks_{};
    std::array<enki::Dependency, capacity> dependencies_{};
    std::atomic<bool> cancelled_{};
    std::size_t size_{};
    bool running_{};
    bool closed_{};
};
} // namespace ngen
