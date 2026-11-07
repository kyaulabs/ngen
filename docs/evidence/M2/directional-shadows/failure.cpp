#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>
#include <vulkan/vulkan.h>

namespace {
bool mesh{}, recorded{};
unsigned shadow_pipelines{};
bool mode(const char *value) {
    const auto *selected = std::getenv("NGEN_SHADOW_CONTROL");
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
    return reinterpret_cast<PFN_vkCreateDescriptorSetLayout>(
        dlsym(RTLD_NEXT, "vkCreateDescriptorSetLayout"))(device, info, allocation, result);
}
extern "C" VkResult vkCreateGraphicsPipelines(VkDevice device, VkPipelineCache cache,
                                              std::uint32_t count,
                                              const VkGraphicsPipelineCreateInfo *info,
                                              const VkAllocationCallbacks *allocation,
                                              VkPipeline *result) {
    const auto next = reinterpret_cast<PFN_vkCreateGraphicsPipelines>(
        dlsym(RTLD_NEXT, "vkCreateGraphicsPipelines"));
    const auto *rendering = static_cast<const VkPipelineRenderingCreateInfo *>(info->pNext);
    if (!mesh || count != 1 || !rendering ||
        rendering->sType != VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO ||
        rendering->colorAttachmentCount != 0)
        return next(device, cache, count, info, allocation, result);
    ++shadow_pipelines;
    if ((mode("pipeline-first") && shadow_pipelines == 1) ||
        (mode("pipeline-last") && shadow_pipelines == 3))
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    if (mode("depth")) {
        auto changed = *info;
        auto depth = *info->pDepthStencilState;
        depth.depthTestEnable = depth.depthWriteEnable = VK_FALSE;
        changed.pDepthStencilState = &depth;
        return next(device, cache, count, &changed, allocation, result);
    }
    if (mode("facing")) {
        auto changed = *info;
        auto raster = *info->pRasterizationState;
        raster.cullMode = VK_CULL_MODE_NONE;
        changed.pRasterizationState = &raster;
        return next(device, cache, count, &changed, allocation, result);
    }
    if (mode("uv1")) {
        auto changed = *info;
        auto vertex = *info->pVertexInputState;
        std::array<VkVertexInputAttributeDescription, 3> attributes{};
        if (vertex.vertexAttributeDescriptionCount != attributes.size())
            std::abort();
        for (std::size_t i = 0; i < attributes.size(); ++i)
            attributes[i] = vertex.pVertexAttributeDescriptions[i];
        attributes[2].offset = attributes[1].offset; // Valid bytes, wrong UV set.
        vertex.pVertexAttributeDescriptions = attributes.data();
        changed.pVertexInputState = &vertex;
        return next(device, cache, count, &changed, allocation, result);
    }
    return next(device, cache, count, info, allocation, result);
}
extern "C" VkResult vkCreateSampler(VkDevice device, const VkSamplerCreateInfo *info,
                                    const VkAllocationCallbacks *allocation, VkSampler *result) {
    auto changed = *info;
    if (mesh && mode("sampler") && !info->compareEnable && info->minFilter == VK_FILTER_NEAREST &&
        info->addressModeU == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE &&
        info->addressModeV == VK_SAMPLER_ADDRESS_MODE_REPEAT)
        changed.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    return reinterpret_cast<PFN_vkCreateSampler>(dlsym(RTLD_NEXT, "vkCreateSampler"))(
        device, &changed, allocation, result);
}
extern "C" void vkCmdDrawIndexed(VkCommandBuffer commands, std::uint32_t indices,
                                 std::uint32_t instances, std::uint32_t first, std::int32_t offset,
                                 std::uint32_t instance) {
    recorded = true;
    reinterpret_cast<PFN_vkCmdDrawIndexed>(dlsym(RTLD_NEXT, "vkCmdDrawIndexed"))(
        commands, indices, instances, first, offset, instance);
}
extern "C" VkResult vkQueueSubmit2(VkQueue queue, std::uint32_t count, const VkSubmitInfo2 *info,
                                   VkFence fence) {
    if (recorded && mode("submit"))
        return VK_ERROR_OUT_OF_HOST_MEMORY;
    return reinterpret_cast<PFN_vkQueueSubmit2>(dlsym(RTLD_NEXT, "vkQueueSubmit2"))(queue, count,
                                                                                    info, fence);
}
