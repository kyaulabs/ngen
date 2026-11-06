// Process-local Vulkan boundary injection; never installed into the renderer.
#include <cstdint>
#include <cstdlib>
#include <vulkan/vulkan.h>

#if defined(NGEN_EXCESS_FORMATS)
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceFormatsKHR(
    VkPhysicalDevice, VkSurfaceKHR, std::uint32_t *count, VkSurfaceFormatKHR *formats) {
    if (formats)
        std::abort(); // A rejected count must never reach a fill/allocation attempt.
    *count = 257;
    return VK_SUCCESS;
}
#else
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice, VkSurfaceKHR, VkSurfaceCapabilitiesKHR *) {
    return VK_ERROR_SURFACE_LOST_KHR;
}
#endif
