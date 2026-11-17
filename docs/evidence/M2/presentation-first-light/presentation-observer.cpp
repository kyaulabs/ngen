#include <cstdint>
#include <cstdio>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

namespace {
unsigned created = 0;
unsigned queued = 0;
bool present_fences = true;
bool early_idle = false;
} // namespace
extern "C" VKAPI_ATTR VkResult VKAPI_CALL
vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *info,
                     const VkAllocationCallbacks *allocator, VkSwapchainKHR *swapchain) {
    const auto real =
        reinterpret_cast<PFN_vkCreateSwapchainKHR>(dlsym(RTLD_NEXT, "vkCreateSwapchainKHR"));
    const auto result = real(device, info, allocator, swapchain);
    if (result == VK_SUCCESS) {
        ++created;
        std::printf("observed_swapchain=%u extent=%ux%u\n", created, info->imageExtent.width,
                    info->imageExtent.height);
    }
    return result;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueuePresentKHR(VkQueue queue,
                                                            const VkPresentInfoKHR *info) {
    bool fenced = false;
    for (auto *node = static_cast<const VkBaseInStructure *>(info->pNext); node;
         node = node->pNext) {
        if (node->sType == VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT) {
            const auto *fences = reinterpret_cast<const VkSwapchainPresentFenceInfoEXT *>(node);
            fenced = fences->swapchainCount == info->swapchainCount && fences->pFences &&
                     fences->pFences[0];
        }
    }
    present_fences &= fenced;
    const auto real =
        reinterpret_cast<PFN_vkQueuePresentKHR>(dlsym(RTLD_NEXT, "vkQueuePresentKHR"));
    const auto result = real(queue, info);
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR)
        ++queued;
    return result;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device) {
    early_idle |= queued < 96;
    const auto real = reinterpret_cast<PFN_vkDeviceWaitIdle>(dlsym(RTLD_NEXT, "vkDeviceWaitIdle"));
    const auto result = real(device);
    std::printf("observed_frames=%u swapchains=%u fenced=%u early_idle=%u\n", queued, created,
                present_fences ? 1U : 0U, early_idle ? 1U : 0U);
    if (result == VK_SUCCESS && (created < 3 || queued != 96 || !present_fences || early_idle))
        return VK_ERROR_VALIDATION_FAILED_EXT;
    return result;
}
