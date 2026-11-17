#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *info,
                                                           const VkAllocationCallbacks *allocator,
                                                           VkInstance *instance) {
    bool validation = false;
    for (std::uint32_t i = 0; i < info->enabledLayerCount; ++i)
        validation |= std::strcmp(info->ppEnabledLayerNames[i], "VK_LAYER_KHRONOS_validation") == 0;
    bool enabled = false;
    for (auto *node = static_cast<const VkBaseInStructure *>(info->pNext); node;
         node = node->pNext) {
        if (node->sType != VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT)
            continue;
        const auto *features = reinterpret_cast<const VkValidationFeaturesEXT *>(node);
        for (std::uint32_t i = 0; i < features->enabledValidationFeatureCount; ++i)
            enabled |= features->pEnabledValidationFeatures[i] ==
                       VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    }
    if (validation && !enabled) {
        std::fputs("shader fixture synchronization validation was not enabled\n", stderr);
        return VK_ERROR_VALIDATION_FAILED_EXT;
    }
    const auto real = reinterpret_cast<PFN_vkCreateInstance>(dlsym(RTLD_NEXT, "vkCreateInstance"));
    return real(info, allocator, instance);
}
