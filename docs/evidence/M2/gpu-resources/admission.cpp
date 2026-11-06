#include <cstdio>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

namespace {
unsigned queries{};
unsigned objects{};
} // namespace
#ifdef NGEN_IMAGE_ADMISSION
extern "C" VKAPI_ATTR void VKAPI_CALL
vkGetDeviceImageMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements *info,
                                   VkMemoryRequirements2 *requirements) {
    const auto next = reinterpret_cast<PFN_vkGetDeviceImageMemoryRequirements>(
        dlsym(RTLD_NEXT, "vkGetDeviceImageMemoryRequirements"));
    next(device, info, requirements);
    ++queries;
    requirements->memoryRequirements.size = 256ULL * 1024 * 1024 + 65536;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(VkDevice device,
                                                        const VkImageCreateInfo *info,
                                                        const VkAllocationCallbacks *callbacks,
                                                        VkImage *image) {
    ++objects;
    const auto next = reinterpret_cast<PFN_vkCreateImage>(dlsym(RTLD_NEXT, "vkCreateImage"));
    return next(device, info, callbacks, image);
}
#else
extern "C" VKAPI_ATTR void VKAPI_CALL
vkGetDeviceBufferMemoryRequirements(VkDevice device, const VkDeviceBufferMemoryRequirements *info,
                                    VkMemoryRequirements2 *requirements) {
    const auto next = reinterpret_cast<PFN_vkGetDeviceBufferMemoryRequirements>(
        dlsym(RTLD_NEXT, "vkGetDeviceBufferMemoryRequirements"));
    next(device, info, requirements);
    ++queries;
    requirements->memoryRequirements.size = 256ULL * 1024 * 1024 + 65536;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateBuffer(VkDevice device,
                                                         const VkBufferCreateInfo *info,
                                                         const VkAllocationCallbacks *callbacks,
                                                         VkBuffer *buffer) {
    ++objects;
    const auto next = reinterpret_cast<PFN_vkCreateBuffer>(dlsym(RTLD_NEXT, "vkCreateBuffer"));
    return next(device, info, callbacks, buffer);
}
#endif
__attribute__((destructor)) static void report() {
#ifdef NGEN_IMAGE_ADMISSION
    std::fprintf(stderr, "preflight_queries=%u native_images=%u\n", queries, objects);
#else
    std::fprintf(stderr, "preflight_queries=%u native_buffers=%u\n", queries, objects);
#endif
}
