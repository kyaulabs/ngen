// Derived from gpu-resources/admission.cpp; current public limit, no native allocation.
#include "render/resources.hpp"
#include <cstdio>
#include <dlfcn.h>
namespace {
unsigned queries{}, objects{};
}
#if defined(NGEN_IMAGE_ADMISSION)
extern "C" VKAPI_ATTR void VKAPI_CALL
vkGetDeviceImageMemoryRequirements(VkDevice device, const VkDeviceImageMemoryRequirements *info,
                                   VkMemoryRequirements2 *requirements) {
    reinterpret_cast<PFN_vkGetDeviceImageMemoryRequirements>(
        dlsym(RTLD_NEXT, "vkGetDeviceImageMemoryRequirements"))(device, info, requirements);
    ++queries;
    requirements->memoryRequirements.size = ngen::render::resource_allocation_limit + 65536;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateImage(VkDevice device,
                                                        const VkImageCreateInfo *info,
                                                        const VkAllocationCallbacks *allocation,
                                                        VkImage *image) {
    ++objects;
    return reinterpret_cast<PFN_vkCreateImage>(dlsym(RTLD_NEXT, "vkCreateImage"))(
        device, info, allocation, image);
}
#else
extern "C" VKAPI_ATTR void VKAPI_CALL
vkGetDeviceBufferMemoryRequirements(VkDevice device, const VkDeviceBufferMemoryRequirements *info,
                                    VkMemoryRequirements2 *requirements) {
    reinterpret_cast<PFN_vkGetDeviceBufferMemoryRequirements>(
        dlsym(RTLD_NEXT, "vkGetDeviceBufferMemoryRequirements"))(device, info, requirements);
    ++queries;
    requirements->memoryRequirements.size = ngen::render::resource_allocation_limit + 65536;
}
extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateBuffer(VkDevice device,
                                                         const VkBufferCreateInfo *info,
                                                         const VkAllocationCallbacks *allocation,
                                                         VkBuffer *buffer) {
    ++objects;
    return reinterpret_cast<PFN_vkCreateBuffer>(dlsym(RTLD_NEXT, "vkCreateBuffer"))(
        device, info, allocation, buffer);
}
#endif
__attribute__((destructor)) static void report() {
    std::fprintf(stderr, "preflight_queries=%u native_objects=%u\n", queries, objects);
}
