#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

extern "C" VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(const VkInstanceCreateInfo *info,
                                                           const VkAllocationCallbacks *allocator,
                                                           VkInstance *instance) {
    bool presentation = false;
    for (std::uint32_t i = 0; i < info->enabledExtensionCount; ++i)
        presentation |= std::strcmp(info->ppEnabledExtensionNames[i],
                                    VK_EXT_SURFACE_MAINTENANCE_1_EXTENSION_NAME) == 0;
    bool enabled = false;
    for (auto *node = static_cast<const VkBaseInStructure *>(info->pNext); node;
         node = node->pNext) {
        if (node->sType != VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT)
            continue;
        auto *features = reinterpret_cast<const VkValidationFeaturesEXT *>(node);
        for (std::uint32_t i = 0; i < features->enabledValidationFeatureCount; ++i)
            enabled |= features->pEnabledValidationFeatures[i] ==
                       VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT;
    }
    if (presentation && !enabled)
        return VK_ERROR_VALIDATION_FAILED_EXT;
    const auto real = reinterpret_cast<PFN_vkCreateInstance>(dlsym(RTLD_NEXT, "vkCreateInstance"));
    return real(info, allocator, instance);
}
