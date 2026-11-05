#include <vulkan/vulkan.h>

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice) {
    return VK_ERROR_DEVICE_LOST;
}
