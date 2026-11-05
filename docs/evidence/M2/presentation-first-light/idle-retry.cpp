#include <cstdio>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

namespace {
unsigned waits = 0;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkDeviceWaitIdle(VkDevice device) {
    const auto real = reinterpret_cast<PFN_vkDeviceWaitIdle>(dlsym(RTLD_NEXT, "vkDeviceWaitIdle"));
    const auto result = real(device);
    ++waits;
    return waits == 1 && result == VK_SUCCESS ? VK_ERROR_OUT_OF_HOST_MEMORY : result;
}
extern "C" VKAPI_ATTR void VKAPI_CALL vkDestroyDevice(VkDevice device,
                                                      const VkAllocationCallbacks *allocator) {
    std::printf("idle_waits=%u destroyed_before_retry=%u\n", waits, waits < 2 ? 1U : 0U);
    const auto real = reinterpret_cast<PFN_vkDestroyDevice>(dlsym(RTLD_NEXT, "vkDestroyDevice"));
    real(device, allocator);
}
