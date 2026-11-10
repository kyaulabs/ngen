// Diagnostic-only system-boundary experiment, not a production GPU owner.
#include <vulkan/vulkan.h>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
namespace {
thread_local VkDevice pending_device{};
thread_local VkSwapchainKHR pending_swapchain{};
template <class T> T next(const char *name) {
    auto result = reinterpret_cast<T>(dlsym(RTLD_NEXT, name));
    if (!result)
        std::abort();
    return result;
}
void flush() {
    if (pending_swapchain)
        next<PFN_vkDestroySwapchainKHR>("vkDestroySwapchainKHR")(
            pending_device, pending_swapchain, nullptr);
    pending_device = VK_NULL_HANDLE;
    pending_swapchain = VK_NULL_HANDLE;
}
}
extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroySwapchainKHR(
    VkDevice device, VkSwapchainKHR swapchain, const VkAllocationCallbacks *allocation) {
    // This workload uses default allocation callbacks and one owner thread.
    if (allocation || pending_swapchain)
        std::abort();
    pending_device = device;
    pending_swapchain = swapchain;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(
    VkDevice device, const VkSwapchainCreateInfoKHR *info,
    const VkAllocationCallbacks *allocation, VkSwapchainKHR *swapchain) {
    auto handoff = *info;
    if (pending_swapchain) {
        if (device != pending_device || info->oldSwapchain)
            std::abort();
        handoff.oldSwapchain = pending_swapchain;
        std::fprintf(stderr, "diagnostic_old_swapchain_handoff=1\n");
    }
    const auto result = next<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")(
        device, &handoff, allocation, swapchain);
    flush();
    return result;
}
extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(
    VkDevice device, const VkAllocationCallbacks *allocation) {
    if (pending_swapchain && pending_device != device)
        std::abort();
    flush();
    next<PFN_vkDestroyDevice>("vkDestroyDevice")(device, allocation);
}
