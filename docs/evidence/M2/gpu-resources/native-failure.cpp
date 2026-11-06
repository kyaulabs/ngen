#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

namespace {
bool fail(const char *api, unsigned &calls, bool persistent = false) {
    ++calls;
    const auto *selected = std::getenv("NGEN_RESOURCE_FAULT_API");
    const auto *ordinal = std::getenv("NGEN_RESOURCE_FAULT_AT");
    if (!selected || !ordinal || std::strcmp(selected, api) != 0)
        return false;
    char *end = nullptr;
    const auto expected = std::strtoul(ordinal, &end, 10);
    if (*ordinal == '\0' || *end != '\0' || expected == 0 ||
        (persistent ? calls < expected : calls != expected))
        return false;
    std::fprintf(stderr, "injected_resource_failure=%s:%u\n", api, calls);
    return true;
}
} // namespace
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAllocateMemory(VkDevice device,
                                                           const VkMemoryAllocateInfo *info,
                                                           const VkAllocationCallbacks *callbacks,
                                                           VkDeviceMemory *memory) {
    static unsigned calls = 0;
    if (fail("memory", calls, true))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    const auto next = reinterpret_cast<PFN_vkAllocateMemory>(dlsym(RTLD_NEXT, "vkAllocateMemory"));
    return next(device, info, callbacks, memory);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateBuffer(VkDevice device,
                                                         const VkBufferCreateInfo *info,
                                                         const VkAllocationCallbacks *callbacks,
                                                         VkBuffer *buffer) {
    static unsigned calls = 0;
    if (fail("buffer", calls))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    const auto next = reinterpret_cast<PFN_vkCreateBuffer>(dlsym(RTLD_NEXT, "vkCreateBuffer"));
    return next(device, info, callbacks, buffer);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(VkDevice device,
                                                        const VkImageCreateInfo *info,
                                                        const VkAllocationCallbacks *callbacks,
                                                        VkImage *image) {
    static unsigned calls = 0;
    if (fail("image", calls))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    const auto next = reinterpret_cast<PFN_vkCreateImage>(dlsym(RTLD_NEXT, "vkCreateImage"));
    return next(device, info, callbacks, image);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkAllocateCommandBuffers(
    VkDevice device, const VkCommandBufferAllocateInfo *info, VkCommandBuffer *commands) {
    static unsigned calls = 0;
    if (fail("commands", calls))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    const auto next = reinterpret_cast<PFN_vkAllocateCommandBuffers>(
        dlsym(RTLD_NEXT, "vkAllocateCommandBuffers"));
    return next(device, info, commands);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBindBufferMemory(VkDevice device, VkBuffer buffer,
                                                             VkDeviceMemory memory,
                                                             VkDeviceSize offset) {
    static unsigned calls = 0;
    if (fail("bind-buffer", calls))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    const auto next =
        reinterpret_cast<PFN_vkBindBufferMemory>(dlsym(RTLD_NEXT, "vkBindBufferMemory"));
    return next(device, buffer, memory, offset);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkBindImageMemory(VkDevice device, VkImage image,
                                                            VkDeviceMemory memory,
                                                            VkDeviceSize offset) {
    static unsigned calls = 0;
    if (fail("bind-image", calls))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    const auto next =
        reinterpret_cast<PFN_vkBindImageMemory>(dlsym(RTLD_NEXT, "vkBindImageMemory"));
    return next(device, image, memory, offset);
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkQueueSubmit2(VkQueue queue, std::uint32_t count,
                                                         const VkSubmitInfo2 *info, VkFence fence) {
    static unsigned calls = 0;
    if (fail("submit", calls))
        return VK_ERROR_OUT_OF_DEVICE_MEMORY;
    const auto next = reinterpret_cast<PFN_vkQueueSubmit2>(dlsym(RTLD_NEXT, "vkQueueSubmit2"));
    return next(queue, count, info, fence);
}
