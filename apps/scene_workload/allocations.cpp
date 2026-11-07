#include "scene_workload/allocations.hpp"
#include <atomic>
#include <cstdlib>
#include <limits>
#include <new>
#if defined(_WIN32)
#include <malloc.h>
#endif

namespace {
std::atomic<std::uint64_t> calls{};
#if NGEN_ALLOCATION_PROBE
void allocation_failed() {
    const auto handler = std::get_new_handler();
    if (!handler)
        throw std::bad_alloc{};
    handler();
}
void *allocate(std::size_t size) {
    calls.fetch_add(1, std::memory_order_relaxed);
    for (;;) {
        if (auto *value = std::malloc(size ? size : 1))
            return value;
        allocation_failed();
    }
}
void *allocate_aligned(std::size_t size, std::align_val_t requested) {
    calls.fetch_add(1, std::memory_order_relaxed);
    const auto alignment = static_cast<std::size_t>(requested);
    if (alignment == 0 || (alignment & (alignment - 1)) != 0)
        throw std::bad_alloc{};
    size = size ? size : 1;
    for (;;) {
        void *value = nullptr;
        if (size <= std::numeric_limits<std::size_t>::max() - (alignment - 1)) {
#if defined(_WIN32)
            value = _aligned_malloc(size, alignment);
#else
            value = std::aligned_alloc(alignment, (size + alignment - 1) / alignment * alignment);
#endif
        }
        if (value)
            return value;
        allocation_failed();
    }
}
void free_aligned(void *value) noexcept {
#if defined(_WIN32)
    _aligned_free(value);
#else
    std::free(value);
#endif
}
#endif
} // namespace
namespace ngen::workload {
std::uint64_t allocation_count() noexcept { return calls.load(std::memory_order_relaxed); }
bool allocation_probe_enabled() noexcept { return NGEN_ALLOCATION_PROBE != 0; }
} // namespace ngen::workload
#if NGEN_ALLOCATION_PROBE
void *operator new(std::size_t size) { return allocate(size); }
void *operator new[](std::size_t size) { return allocate(size); }
void *operator new(std::size_t size, std::align_val_t alignment) {
    return allocate_aligned(size, alignment);
}
void *operator new[](std::size_t size, std::align_val_t alignment) {
    return allocate_aligned(size, alignment);
}
void operator delete(void *value) noexcept { std::free(value); }
void operator delete[](void *value) noexcept { std::free(value); }
void operator delete(void *value, std::size_t) noexcept { std::free(value); }
void operator delete[](void *value, std::size_t) noexcept { std::free(value); }
void operator delete(void *value, std::align_val_t) noexcept { free_aligned(value); }
void operator delete[](void *value, std::align_val_t) noexcept { free_aligned(value); }
void operator delete(void *value, std::size_t, std::align_val_t) noexcept { free_aligned(value); }
void operator delete[](void *value, std::size_t, std::align_val_t) noexcept { free_aligned(value); }
#endif
