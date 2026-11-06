#include <dlfcn.h>
#include <vulkan/vulkan.h>

#if defined(NGEN_EXCESS_IMAGES)
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
    VkPhysicalDevice device, VkSurfaceKHR surface, VkSurfaceCapabilitiesKHR *capabilities) {
    const auto real = reinterpret_cast<PFN_vkGetPhysicalDeviceSurfaceCapabilitiesKHR>(
        dlsym(RTLD_NEXT, "vkGetPhysicalDeviceSurfaceCapabilitiesKHR"));
    const auto result = real(device, surface, capabilities);
    if (result == VK_SUCCESS) {
        capabilities->minImageCount = 17;
        capabilities->maxImageCount = 0;
    }
    return result;
}
#else
extern "C" VKAPI_ATTR void VKAPI_CALL
vkGetPhysicalDeviceFeatures2(VkPhysicalDevice device, VkPhysicalDeviceFeatures2 *features) {
    const auto real = reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>(
        dlsym(RTLD_NEXT, "vkGetPhysicalDeviceFeatures2"));
    real(device, features);
    for (auto *node = static_cast<VkBaseOutStructure *>(features->pNext); node;
         node = node->pNext) {
        if (node->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SWAPCHAIN_MAINTENANCE_1_FEATURES_EXT)
            reinterpret_cast<VkPhysicalDeviceSwapchainMaintenance1FeaturesEXT *>(node)
                ->swapchainMaintenance1 = VK_FALSE;
    }
}
#endif
