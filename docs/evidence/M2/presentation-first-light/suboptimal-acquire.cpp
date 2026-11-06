#include <cstdint>
#include <dlfcn.h>
#include <vulkan/vulkan.h>
namespace {
VkFence acquired{};
bool delay{};
} // namespace
extern "C" VKAPI_ATTR VkResult VKAPI_CALL
vkAcquireNextImageKHR(VkDevice device, VkSwapchainKHR swapchain, std::uint64_t timeout,
                      VkSemaphore semaphore, VkFence fence, std::uint32_t *index) {
    const auto real =
        reinterpret_cast<PFN_vkAcquireNextImageKHR>(dlsym(RTLD_NEXT, "vkAcquireNextImageKHR"));
    const auto result = real(device, swapchain, timeout, semaphore, fence, index);
    if (result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR) {
        acquired = fence;
        delay = true;
        return VK_SUBOPTIMAL_KHR;
    }
    return result;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetFenceStatus(VkDevice device, VkFence fence) {
    if (fence == acquired && delay) {
        delay = false;
        return VK_NOT_READY;
    }
    const auto real = reinterpret_cast<PFN_vkGetFenceStatus>(dlsym(RTLD_NEXT, "vkGetFenceStatus"));
    return real(device, fence);
}
