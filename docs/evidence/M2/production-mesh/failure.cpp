#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

namespace {
bool mesh{};
bool recorded{};
unsigned pipelines{};
bool mode(const char *value) {
    const auto *selected = std::getenv("NGEN_MESH_CONTROL");
    return selected && std::strcmp(selected, value) == 0;
}
} // namespace
extern "C" VkResult vkCreateDescriptorSetLayout(VkDevice device,
                                                const VkDescriptorSetLayoutCreateInfo *info,
                                                const VkAllocationCallbacks *allocation,
                                                VkDescriptorSetLayout *result) {
    if (info->bindingCount == 4 &&
        info->pBindings[0].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER &&
        info->pBindings[1].descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        mesh = true;
    const auto next = reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(
        dlsym(RTLD_NEXT, "vkCreateDescriptorSetLayout"));
    return next(device, info, allocation, result);
}
extern "C" VkResult vkAllocateDescriptorSets(VkDevice device,
                                             const VkDescriptorSetAllocateInfo *info,
                                             VkDescriptorSet *sets) {
    if (mesh && mode("sets"))
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    return reinterpret_cast<PFN_vkAllocateDescriptorSets>(
        dlsym(RTLD_NEXT, "vkAllocateDescriptorSets"))(device, info, sets);
}
extern "C" VkResult vkCreateSampler(VkDevice device, const VkSamplerCreateInfo *info,
                                    const VkAllocationCallbacks *allocation, VkSampler *result) {
    if (mesh && mode("sampler"))
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    return reinterpret_cast<PFN_vkCreateSampler>(dlsym(RTLD_NEXT, "vkCreateSampler"))(
        device, info, allocation, result);
}
extern "C" VkResult vkCreateShaderModule(VkDevice device, const VkShaderModuleCreateInfo *info,
                                         const VkAllocationCallbacks *allocation,
                                         VkShaderModule *result) {
    if (mesh && mode("module"))
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    return reinterpret_cast<PFN_vkCreateShaderModule>(dlsym(RTLD_NEXT, "vkCreateShaderModule"))(
        device, info, allocation, result);
}
extern "C" VkResult vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache cache,
                                              std::uint32_t count,
                                              const VkGraphicsPipelineCreateInfo *info,
                                              const VkAllocationCallbacks *allocation,
                                              VkPipeline *result) {
    const auto next = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(
        dlsym(RTLD_NEXT, "vkCreateGraphicsPipelines"));
    if (!mesh)
        return next(device, cache, count, info, allocation, result);
    ++pipelines;
    if (mode("pipeline2") && pipelines == 2)
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    if (mode("no-depth") && count == 1 && info->pDepthStencilState) {
        auto changed = *info;
        auto depth = *info->pDepthStencilState;
        depth.depthTestEnable = VK_FALSE;
        changed.pDepthStencilState = &depth;
        return next(device, cache, count, &changed, allocation, result);
    }
    return next(device, cache, count, info, allocation, result);
}
extern "C" void vkCmdDrawIndexed(VkCommandBuffer commands, std::uint32_t indices,
                                 std::uint32_t instances, std::uint32_t first, std::int32_t offset,
                                 std::uint32_t instance) {
    recorded = true;
    reinterpret_cast<PFN_vkCmdDrawIndexed>(dlsym(RTLD_NEXT, "vkCmdDrawIndexed"))(
        commands, indices, instances, mode("index") ? 0 : first, offset, instance);
}
extern "C" VkResult vkQueueSubmit2(VkQueue queue, std::uint32_t count, const VkSubmitInfo2 *info,
                                   VkFence fence) {
    if (recorded && mode("submit"))
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    return reinterpret_cast<PFN_vkQueueSubmit2>(dlsym(RTLD_NEXT, "vkQueueSubmit2"))(queue, count,
                                                                                    info, fence);
}
