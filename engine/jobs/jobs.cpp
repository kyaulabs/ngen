#include "jobs/jobs.hpp"
#include <stdexcept>
namespace ngen {
namespace {
std::atomic<std::uint64_t> allocations{};
void *allocate(std::size_t alignment, std::size_t size, void *user, const char *file, int line) {
    ++allocations;
    return enki::DefaultAllocFunc(alignment, size, user, file, line);
}
} // namespace
std::uint64_t Jobs::allocation_count() noexcept { return allocations.load(); }
Jobs::Jobs(unsigned workers) {
    if (workers > 7)
        throw std::invalid_argument("NGEN supports 0..7 worker threads");
    enki::TaskSchedulerConfig config;
    config.numTaskThreadsToCreate = workers;
    config.customAllocator.alloc = allocate;
    scheduler_.Initialize(config);
}
Jobs::~Jobs() { shutdown(); }
void Jobs::Task::ExecuteRange(enki::TaskSetPartition, std::uint32_t) {
    if (!cancelled->load())
        function(context, *cancelled);
}
Result<std::size_t> Jobs::submit(Function function, void *context, std::size_t after) {
    if (closed_)
        return Error::closed;
    if (running_)
        return Error::busy;
    if (size_ == capacity)
        return Error::capacity;
    if (!function || (after != no_dependency && after >= size_))
        return Error::invalid;
    auto &task = tasks_[size_];
    task.function = function;
    task.context = context;
    task.cancelled = &cancelled_;
    task.after = after;
    return size_++;
}
Result<std::monostate> Jobs::start() {
    if (closed_)
        return Error::closed;
    if (running_)
        return Error::busy;
    running_ = true;
    for (std::size_t i = 0; i < size_; ++i) {
        if (tasks_[i].after != no_dependency)
            dependencies_[i].SetDependency(&tasks_[tasks_[i].after], &tasks_[i]);
    }
    for (std::size_t i = 0; i < size_; ++i) {
        if (tasks_[i].after == no_dependency)
            scheduler_.AddTaskSetToPipe(&tasks_[i]);
    }
    return std::monostate{};
}
void Jobs::request_cancel() noexcept { cancelled_.store(true); }
void Jobs::drain() {
    if (running_)
        scheduler_.WaitforAll();
    for (std::size_t i = 0; i < size_; ++i) {
        dependencies_[i].ClearDependency();
        tasks_[i].context = nullptr;
        tasks_[i].function = nullptr;
    }
    size_ = 0;
    running_ = false;
    cancelled_.store(false);
}
void Jobs::shutdown() {
    if (closed_)
        return;
    request_cancel();
    drain();
    scheduler_.WaitforAllAndShutdown();
    closed_ = true;
}
} // namespace ngen
