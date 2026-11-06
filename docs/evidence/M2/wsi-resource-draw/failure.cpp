// Process-local controls target the two-signal resource/WSI submission only.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vulkan/vulkan.h>
namespace {
std::atomic<bool> queued{};
bool mode(const char *name) {
    const auto *value = std::getenv("NGEN_WSI_RESOURCE_FAILURE");
    return value && std::strcmp(value, name) == 0;
}
VkResult fail() {
    std::fputs("wsi_resource_failure_injected=1\n", stderr);
    return VK_ERROR_OUT_OF_HOST_MEMORY;
}
} // namespace
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2(VkQueue queue, std::uint32_t count,
                                                         const VkSubmitInfo2 *submits,
                                                         VkFence fence) {
    const bool resource_frame = count == 1 && submits[0].signalSemaphoreInfoCount == 2;
    if (resource_frame && mode("submit"))
        return fail();
    const auto result = reinterpret_cast<PFN_vkQueueSubmit2>(dlsym(RTLD_NEXT, "vkQueueSubmit2"))(
        queue, count, submits, fence);
    if (resource_frame && result == VK_SUCCESS)
        queued = true;
    return result;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkResetFences(VkDevice device, std::uint32_t count,
                                                        const VkFence *fences) {
    if (queued.exchange(false) && mode("retirement"))
        return fail();
    return reinterpret_cast<PFN_vkResetFences>(dlsym(RTLD_NEXT, "vkResetFences"))(device, count,
                                                                                  fences);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue,
                                                            const VkPresentInfoKHR *info) {
    if (mode("present"))
        return fail();
    return reinterpret_cast<PFN_vkQueuePresentKHR>(dlsym(RTLD_NEXT, "vkQueuePresentKHR"))(queue,
                                                                                          info);
}
