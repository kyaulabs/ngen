#include <cstdio>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

namespace {
unsigned accepted = 0;
bool injected = false;
} // namespace
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue,
                                                            const VkPresentInfoKHR *info) {
    const auto real =
        reinterpret_cast<PFN_vkQueuePresentKHR>(dlsym(RTLD_NEXT, "vkQueuePresentKHR"));
    const auto result = real(queue, info);
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
        if (!injected) {
            injected = true;
            // Real queue operations still occur, as required for an out-of-date result.
            return VK_ERROR_OUT_OF_DATE_KHR;
        }
        ++accepted;
    }
    return result;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device) {
    const auto real = reinterpret_cast<PFN_vkDeviceWaitIdle>(dlsym(RTLD_NEXT, "vkDeviceWaitIdle"));
    const auto result = real(device);
    std::printf("accepted_after_outdated=%u\n", accepted);
    return result == VK_SUCCESS && accepted < 96 ? VK_ERROR_VALIDATION_FAILED_EXT : result;
}
