// Diagnostic-only native boundary timings; never used as a clean performance pass.
#include <SDL3/SDL.h>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <time.h>
#include <vulkan/vulkan.h>
namespace {
std::uint64_t now() {
    timespec t{};
    (void)clock_gettime(CLOCK_MONOTONIC, &t);
    return static_cast<std::uint64_t>(t.tv_sec) * 1000000000ULL + static_cast<std::uint64_t>(t.tv_nsec);
}
void report(const char *name, std::uint64_t begin, std::uint64_t end, int result) {
    if (end - begin > 5000000)
        std::fprintf(stderr, "native_latency name=%s begin_ns=%llu end_ns=%llu duration_ns=%llu result=%d\n", name,
            static_cast<unsigned long long>(begin), static_cast<unsigned long long>(end),
            static_cast<unsigned long long>(end - begin), result);
}
template<class T> T next(const char *name) {
    auto result = reinterpret_cast<T>(dlsym(RTLD_NEXT, name));
    if (!result) std::abort();
    return result;
}
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, std::uint64_t timeout, VkSemaphore semaphore, VkFence fence, std::uint32_t *index) {
    const auto begin = now();
    const auto result = next<PFN_vkAcquireNextImageKHR>("vkAcquireNextImageKHR")(device, swapchain, timeout, semaphore, fence, index);
    const auto end = now();
    report("acquire", begin, end, result);
    static thread_local VkSwapchainKHR previous{};
    static thread_local std::uint64_t pending{};
    if (previous != swapchain) { previous = swapchain; pending = 0; }
    if ((result == VK_NOT_READY || result == VK_TIMEOUT) && !pending) pending = begin;
    if ((result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) && pending) {
        if (end - pending > 100000000) report("acquire_poll_span", pending, end, result);
        pending = 0;
    }
    return result;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceStatus(VkDevice device, VkFence fence) {
    const auto begin = now();
    const auto result = next<PFN_vkGetFenceStatus>("vkGetFenceStatus")(device, fence);
    const auto end = now();
    report("fence_status", begin, end, result);
    struct Entry { VkFence fence{}; std::uint64_t pending{}; };
    static thread_local std::array<Entry, 64> entries;
    for (auto &entry : entries) if (entry.fence == fence || !entry.fence) {
        entry.fence = fence;
        if (result == VK_NOT_READY && !entry.pending) entry.pending = begin;
        if (result == VK_SUCCESS) {
            if (entry.pending && end - entry.pending > 100000000) report("fence_poll_span", entry.pending, end, result);
            entry = {};
        }
        break;
    }
    return result;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR *info) {
    const auto begin = now();
    const auto result = next<PFN_vkQueuePresentKHR>("vkQueuePresentKHR")(queue, info);
    report("present", begin, now(), result);
    return result;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2(VkQueue queue, std::uint32_t count, const VkSubmitInfo2 *info, VkFence fence) {
    const auto begin = now();
    const auto result = next<PFN_vkQueueSubmit2>("vkQueueSubmit2")(queue, count, info, fence);
    report("submit", begin, now(), result);
    return result;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *info, const VkAllocationCallbacks *allocation, VkSwapchainKHR *swapchain) {
    const auto begin = now();
    const auto result = next<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")(device, info, allocation, swapchain);
    report("create_swapchain", begin, now(), result);
    std::fprintf(stderr, "swapchain_created at_ns=%llu width=%u height=%u requested_images=%u result=%d\n",
        static_cast<unsigned long long>(now()), info->imageExtent.width, info->imageExtent.height, info->minImageCount, result);
    return result;
}
extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *allocation) {
    const auto begin = now();
    next<PFN_vkDestroySwapchainKHR>("vkDestroySwapchainKHR")(device, swapchain, allocation);
    report("destroy_swapchain", begin, now(), 0);
}
extern "C" bool SDLCALL SDL_SetWindowSize(SDL_Window *window, int width, int height) {
    static unsigned calls{};
    const auto *control = std::getenv("NGEN_LATENCY_CONTROL");
    if (++calls > 1 && control && std::strcmp(control, "no-resize") == 0) {
        std::fprintf(stderr, "diagnostic_resize_omitted=1 at_ns=%llu\n", static_cast<unsigned long long>(now()));
        return true;
    }
    return next<decltype(&SDL_SetWindowSize)>("SDL_SetWindowSize")(window, width, height);
}
extern "C" bool SDLCALL SDL_PushEvent(SDL_Event *event) {
    const auto *control = std::getenv("NGEN_LATENCY_CONTROL");
    if (control && std::strcmp(control, "no-reload") == 0 &&
        (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP) && event->key.scancode == SDL_SCANCODE_R) {
        std::fprintf(stderr, "diagnostic_reload_omitted=1 at_ns=%llu\n", static_cast<unsigned long long>(now()));
        return true;
    }
    return next<decltype(&SDL_PushEvent)>("SDL_PushEvent")(event);
}
extern "C" bool SDLCALL SDL_PollEvent(SDL_Event *event) {
    const auto begin = now();
    const auto result = next<decltype(&SDL_PollEvent)>("SDL_PollEvent")(event);
    report("poll_event", begin, now(), result ? 1 : 0);
    return result;
}
extern "C" void SDLCALL SDL_PumpEvents() {
    const auto begin = now();
    next<decltype(&SDL_PumpEvents)>("SDL_PumpEvents")();
    report("pump_events", begin, now(), 0);
}
