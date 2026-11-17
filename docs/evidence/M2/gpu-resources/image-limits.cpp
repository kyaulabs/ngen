#include <cstdio>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

namespace {
unsigned queries{};
unsigned images{};
} // namespace
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkGetPhysicalDeviceImageFormatProperties(
    VkPhysicalDevice physical, VkFormat format, VkImageType type, VkImageTiling tiling,
    VkImageUsageFlags usage, VkImageCreateFlags flags, VkImageFormatProperties *properties) {
    ++queries;
#ifdef NGEN_UNSUPPORTED_IMAGE
    (void)physical;
    (void)format;
    (void)type;
    (void)tiling;
    (void)usage;
    (void)flags;
    (void)properties;
    return VK_ERROR_FORMAT_NOT_SUPPORTED;
#else
    const auto next = reinterpret_cast<PFN_vkGetPhysicalDeviceImageFormatProperties>(
        dlsym(RTLD_NEXT, "vkGetPhysicalDeviceImageFormatProperties"));
    const auto result = next(physical, format, type, tiling, usage, flags, properties);
    if (result == VK_SUCCESS)
        properties->maxExtent = {4, 4, 1};
    return result;
#endif
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(VkDevice device,
                                                        const VkImageCreateInfo *info,
                                                        const VkAllocationCallbacks *callbacks,
                                                        VkImage *image) {
    ++images;
    const auto next = reinterpret_cast<PFN_vkCreateImage>(dlsym(RTLD_NEXT, "vkCreateImage"));
    return next(device, info, callbacks, image);
}
__attribute__((destructor)) static void report() {
    std::fprintf(stderr, "image_format_queries=%u native_images=%u\n", queries, images);
}
