// Diagnostic-only count control. It does not change FIFO or drop measured frames.
#include <vulkan/vulkan.h>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
namespace {
thread_local std::uint32_t minimum{};
template <class T> T next(const char *name) {
    auto result = reinterpret_cast<T>(dlsym(RTLD_NEXT, name));
    if (!result)
        std::abort();
    return result;
}
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice device, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *caps) {
    const auto result = next<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
        "vkGetPhysicalDeviceSurfaceCapabilitiesKHR")(device, surface, caps);
    if (result == VK_SUCCESS)
        minimum = caps->minImageCount;
    return result;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSwapchainKHR(
    VkDevice device, const VkSwapchainCreateInfoKHR *info,
    const VkAllocationCallbacks *allocation, VkSwapchainKHR *swapchain) {
    if (!minimum)
        std::abort();
    auto controlled = *info;
    controlled.minImageCount = minimum;
    std::fprintf(stderr, "diagnostic_image_count original=%u minimum=%u\n", info->minImageCount, minimum);
    const auto result = next<PFN_vkCreateSwapchainKHR>("vkCreateSwapchainKHR")(
        device, &controlled, allocation, swapchain);
    if (result == VK_SUCCESS) {
        std::uint32_t actual{};
        const auto count = next<PFN_vkGetSwapchainImagesKHR>("vkGetSwapchainImagesKHR")(
            device, *swapchain, &actual, nullptr);
        std::fprintf(stderr, "diagnostic_image_count actual=%u result=%d\n", actual, count);
    }
    return result;
}
