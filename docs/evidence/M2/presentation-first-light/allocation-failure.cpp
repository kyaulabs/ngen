#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

namespace {
bool fail(const char *api, unsigned &calls) {
    ++calls;
    const char *selected = std::getenv("NGEN_FAULT_API");
    const char *ordinal = std::getenv("NGEN_FAULT_AT");
    if (!selected || !ordinal || std::strcmp(selected, api) != 0)
        return false;
    char *end = nullptr;
    const auto expected = std::strtoul(ordinal, &end, 10);
    if (*ordinal == '\0' || *end != '\0' || expected == 0 || expected != calls)
        return false;
    std::fprintf(stderr, "injected_allocation_failure=%s:%u\n", api, calls);
    return true;
}
} // namespace
extern "C" VKAPI_ATTR VkResult VKAPI_CALL
vkCreateCommandPool(VkDevice device, const VkCommandPoolCreateInfo *info,
                    const VkAllocationCallbacks *allocator, VkCommandPool *pool) {
    static unsigned calls = 0;
    if (fail("pool", calls))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    const auto real =
        reinterpret_cast<PFN_vkCreateCommandPool>(dlsym(RTLD_NEXT, "vkCreateCommandPool"));
    return real(device, info, allocator, pool);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateFence(VkDevice device,
                                                        const VkFenceCreateInfo *info,
                                                        const VkAllocationCallbacks *allocator,
                                                        VkFence *fence) {
    static unsigned calls = 0;
    if (fail("fence", calls))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    const auto real = reinterpret_cast<PFN_vkCreateFence>(dlsym(RTLD_NEXT, "vkCreateFence"));
    return real(device, info, allocator, fence);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateSemaphore(VkDevice device,
                                                            const VkSemaphoreCreateInfo *info,
                                                            const VkAllocationCallbacks *allocator,
                                                            VkSemaphore *semaphore) {
    static unsigned calls = 0;
    if (fail("semaphore", calls))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    const auto real =
        reinterpret_cast<PFN_vkCreateSemaphore>(dlsym(RTLD_NEXT, "vkCreateSemaphore"));
    return real(device, info, allocator, semaphore);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateImageView(VkDevice device,
                                                            const VkImageViewCreateInfo *info,
                                                            const VkAllocationCallbacks *allocator,
                                                            VkImageView *view) {
    static unsigned calls = 0;
    if (fail("view", calls))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    const auto real =
        reinterpret_cast<PFN_vkCreateImageView>(dlsym(RTLD_NEXT, "vkCreateImageView"));
    return real(device, info, allocator, view);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL
vkCreateSwapchainKHR(VkDevice device, const VkSwapchainCreateInfoKHR *info,
                     const VkAllocationCallbacks *allocator, VkSwapchainKHR *swapchain) {
    static unsigned calls = 0;
    if (fail("swapchain", calls))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    const auto real =
        reinterpret_cast<PFN_vkCreateSwapchainKHR>(dlsym(RTLD_NEXT, "vkCreateSwapchainKHR"));
    return real(device, info, allocator, swapchain);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkWaitForFences(VkDevice device, uint32_t count,
                                                          const VkFence *fences, VkBool32 all,
                                                          uint64_t timeout) {
    static unsigned calls = 0;
    if (fail("wait", calls))
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    const auto real = reinterpret_cast<PFN_vkWaitForFences>(dlsym(RTLD_NEXT, "vkWaitForFences"));
    return real(device, count, fences, all, timeout);
}
