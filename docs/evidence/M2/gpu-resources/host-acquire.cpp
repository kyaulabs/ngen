#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

namespace {
struct Observation {
    VkDevice device{};
    VkSemaphore semaphore{};
    std::uint64_t required{};
    std::uint64_t acquired{};
};
std::array<Observation, 2> observations{};
unsigned waits{};
unsigned missing{};
unsigned blocking{};
unsigned device_idles{};
unsigned queue_idles{};
Observation *observe(VkDevice device, VkSemaphore semaphore) {
    for (auto &observation : observations)
        if (observation.device == device && observation.semaphore == semaphore)
            return &observation;
    for (auto &observation : observations)
        if (!observation.device) {
            observation.device = device;
            observation.semaphore = semaphore;
            return &observation;
        }
    std::fputs("resource_observer: bounded observation capacity exhausted\n", stderr);
    return nullptr;
}
} // namespace
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetSemaphoreCounterValue(VkDevice device,
                                                                     VkSemaphore semaphore,
                                                                     std::uint64_t *value) {
    auto *observation = observe(device, semaphore);
    if (!observation || observation->required > observation->acquired) {
        ++missing;
        std::fputs("resource_observer: host acquisition missing after completion query\n", stderr);
        return VK_ERROR_VALIDATION_FAILED_EXT;
    }
    const auto next = reinterpret_cast<PFN_vkGetSemaphoreCounterValue>(
        dlsym(RTLD_NEXT, "vkGetSemaphoreCounterValue"));
    const auto result = next(device, semaphore, value);
    if (result == VK_SUCCESS)
        observation->required = std::max(observation->required, *value);
    return result;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkWaitSemaphores(VkDevice device,
                                                           const VkSemaphoreWaitInfo *info,
                                                           std::uint64_t timeout) {
    ++waits;
    if (timeout != 0)
        ++blocking;
#ifdef NGEN_FAIL_HOST_WAIT
    if (waits == 1)
        return VK_ERROR_UNKNOWN;
#endif
    const auto next = reinterpret_cast<PFN_vkWaitSemaphores>(dlsym(RTLD_NEXT, "vkWaitSemaphores"));
    const auto result = next(device, info, timeout);
    if (result == VK_SUCCESS && info->semaphoreCount == 1 && info->flags == 0) {
        if (auto *observation = observe(device, info->pSemaphores[0]))
            observation->acquired = std::max(observation->acquired, info->pValues[0]);
    }
    return result;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device) {
    ++device_idles;
    const auto next = reinterpret_cast<PFN_vkDeviceWaitIdle>(dlsym(RTLD_NEXT, "vkDeviceWaitIdle"));
    return next(device);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueWaitIdle(VkQueue queue) {
    ++queue_idles;
    const auto next = reinterpret_cast<PFN_vkQueueWaitIdle>(dlsym(RTLD_NEXT, "vkQueueWaitIdle"));
    return next(queue);
}
__attribute__((destructor)) static void report() {
    std::fprintf(stderr,
                 "host_acquires=%u missing_acquires=%u blocking_waits=%u device_idle_calls=%u "
                 "queue_idle_calls=%u\n",
                 waits, missing, blocking, device_idles, queue_idles);
}
