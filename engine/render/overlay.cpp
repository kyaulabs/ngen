#include "render/overlay.hpp"
#include <algorithm>
#include <cstdio>
#include <stdexcept>
#if defined(NGEN_COMPILED_SHADERS)
#include "ngen_shader_code.hpp"
#endif

namespace ngen::render {
namespace {
#if defined(NGEN_COMPILED_SHADERS)
void check(VkResult result, const char *operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
}
VkShaderModule module(VkDevice device, std::span<const std::uint32_t> code) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = code.size_bytes();
    info.pCode = code.data();
    VkShaderModule result{};
    check(vkCreateShaderModule(device, &info, nullptr, &result), "create embedded overlay module");
    return result;
}
#endif
} // namespace
OverlayPipeline::OverlayPipeline(VkDevice device) : device_(device) {
#if !defined(NGEN_COMPILED_SHADERS)
    device_ = VK_NULL_HANDLE;
    throw std::runtime_error("overlay shaders unavailable; enable the offline shader build");
#else
    std::array<VkShaderModule, 2> modules{};
    try {
        const std::array<VkDescriptorSetLayoutBinding, 2> bindings{
            {{0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
             {1, VK_DESCRIPTOR_TYPE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}}};
        VkDescriptorSetLayoutCreateInfo descriptors{};
        descriptors.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        descriptors.bindingCount = static_cast<std::uint32_t>(bindings.size());
        descriptors.pBindings = bindings.data();
        check(vkCreateDescriptorSetLayout(device_, &descriptors, nullptr, &descriptor_layout_),
              "create overlay descriptor layout");
        const VkPushConstantRange push{VK_SHADER_STAGE_VERTEX_BIT, 0,
                                       sizeof(shader::OverlayConstants)};
        VkPipelineLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &descriptor_layout_;
        layout.pushConstantRangeCount = 1;
        layout.pPushConstantRanges = &push;
        check(vkCreatePipelineLayout(device_, &layout, nullptr, &layout_),
              "create overlay pipeline layout");
        const std::array<VkDescriptorPoolSize, 2> sizes{
            {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4}, {VK_DESCRIPTOR_TYPE_SAMPLER, 4}}};
        VkDescriptorPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.maxSets = 4;
        pool.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        pool.pPoolSizes = sizes.data();
        check(vkCreateDescriptorPool(device_, &pool, nullptr, &pool_),
              "create overlay descriptor pool");
        const std::array layouts{descriptor_layout_, descriptor_layout_, descriptor_layout_,
                                 descriptor_layout_};
        VkDescriptorSetAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate.descriptorPool = pool_;
        allocate.descriptorSetCount = static_cast<std::uint32_t>(layouts.size());
        allocate.pSetLayouts = layouts.data();
        check(vkAllocateDescriptorSets(device_, &allocate, sets_.data()),
              "allocate overlay descriptors");
        VkSamplerCreateInfo sampler{};
        sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler.magFilter = sampler.minFilter = VK_FILTER_LINEAR;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU = sampler.addressModeV = sampler.addressModeW =
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        check(vkCreateSampler(device_, &sampler, nullptr, &sampler_), "create overlay sampler");
        modules[0] = module(device_, compiled::overlay_vertex);
        modules[1] = module(device_, compiled::overlay_fragment);
        const auto stage = [](VkShaderStageFlagBits kind, VkShaderModule shader_module) {
            return VkPipelineShaderStageCreateInfo{
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                nullptr,
                0,
                kind,
                shader_module,
                "main",
                nullptr};
        };
        const std::array stages{stage(VK_SHADER_STAGE_VERTEX_BIT, modules[0]),
                                stage(VK_SHADER_STAGE_FRAGMENT_BIT, modules[1])};
        const VkVertexInputBindingDescription binding{0, sizeof(shader::OverlayVertex),
                                                      VK_VERTEX_INPUT_RATE_VERTEX};
        const std::array<VkVertexInputAttributeDescription, 3> attributes{
            {{0, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(shader::OverlayVertex, position)},
             {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(shader::OverlayVertex, uv)},
             {2, 0, VK_FORMAT_R32_UINT, offsetof(shader::OverlayVertex, color)}}};
        VkPipelineVertexInputStateCreateInfo vertex{};
        vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex.vertexBindingDescriptionCount = 1;
        vertex.pVertexBindingDescriptions = &binding;
        vertex.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
        vertex.pVertexAttributeDescriptions = attributes.data();
        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = viewport.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1;
        const VkPipelineMultisampleStateCreateInfo samples{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FALSE,
            0,
            nullptr,
            VK_FALSE,
            VK_FALSE};
        VkPipelineColorBlendAttachmentState color{};
        color.colorWriteMask = static_cast<VkColorComponentFlags>(VK_COLOR_COMPONENT_R_BIT) |
                               VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                               VK_COLOR_COMPONENT_A_BIT;
        color.blendEnable = VK_TRUE;
        color.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        color.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color.colorBlendOp = VK_BLEND_OP_ADD;
        color.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        color.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        color.alphaBlendOp = VK_BLEND_OP_ADD;
        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments = &color;
        const std::array states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = static_cast<std::uint32_t>(states.size());
        dynamic.pDynamicStates = states.data();
        constexpr std::array formats{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB,
                                     VK_FORMAT_B8G8R8A8_SRGB};
        for (std::size_t i = 0; i < pipelines_.size(); ++i) {
            VkPipelineRenderingCreateInfo rendering{};
            rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            rendering.colorAttachmentCount = 1;
            rendering.pColorAttachmentFormats = &formats[i];
            VkGraphicsPipelineCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            info.pNext = &rendering;
            info.stageCount = static_cast<std::uint32_t>(stages.size());
            info.pStages = stages.data();
            info.pVertexInputState = &vertex;
            info.pInputAssemblyState = &assembly;
            info.pViewportState = &viewport;
            info.pRasterizationState = &raster;
            info.pMultisampleState = &samples;
            info.pColorBlendState = &blend;
            info.pDynamicState = &dynamic;
            info.layout = layout_;
            check(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr,
                                            &pipelines_[i]),
                  "create prewarmed overlay pipeline");
        }
        for (auto shader : modules)
            vkDestroyShaderModule(device_, shader, nullptr);
    } catch (...) {
        for (auto shader : modules)
            if (shader)
                vkDestroyShaderModule(device_, shader, nullptr);
        close();
        throw;
    }
#endif
}
void OverlayPipeline::prepare(std::size_t slot, VkImageView font) const {
    const VkDescriptorImageInfo image{VK_NULL_HANDLE, font,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const VkDescriptorImageInfo sampler{sampler_, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
    std::array<VkWriteDescriptorSet, 2> writes{};
    for (std::size_t i = 0; i < writes.size(); ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = sets_[slot];
        writes[i].dstBinding = static_cast<std::uint32_t>(i);
        writes[i].descriptorCount = 1;
        writes[i].descriptorType =
            i == 0 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
        writes[i].pImageInfo = i == 0 ? &image : &sampler;
    }
    vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
}
void OverlayPipeline::record(const OverlayRecording &recording) const {
    const auto format = recording.format == VK_FORMAT_R8G8B8A8_UNORM  ? 0U
                        : recording.format == VK_FORMAT_R8G8B8A8_SRGB ? 1U
                                                                      : 2U;
    vkCmdBindPipeline(recording.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelines_[format]);
    vkCmdBindDescriptorSets(recording.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1,
                            &sets_[recording.slot], 0, nullptr);
    const VkViewport viewport{0,
                              0,
                              static_cast<float>(recording.extent.width),
                              static_cast<float>(recording.extent.height),
                              0,
                              1};
    vkCmdSetViewport(recording.commands, 0, 1, &viewport);
    const shader::OverlayConstants constants{{2.0F / static_cast<float>(recording.extent.width),
                                              2.0F / static_cast<float>(recording.extent.height),
                                              -1, -1}};
    vkCmdPushConstants(recording.commands, layout_, VK_SHADER_STAGE_VERTEX_BIT, 0,
                       sizeof(constants), &constants);
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(recording.commands, 0, 1, &recording.vertices, &offset);
    vkCmdBindIndexBuffer(recording.commands, recording.indices, 0, VK_INDEX_TYPE_UINT32);
    for (const auto &draw : recording.draws) {
        const auto x = static_cast<std::uint32_t>(draw.clip.offset.x);
        const auto y = static_cast<std::uint32_t>(draw.clip.offset.y);
        const auto right = std::min(recording.extent.width, x + draw.clip.extent.width);
        const auto bottom = std::min(recording.extent.height, y + draw.clip.extent.height);
        if (right <= x || bottom <= y)
            continue;
        const VkRect2D clip{draw.clip.offset, {right - x, bottom - y}};
        vkCmdSetScissor(recording.commands, 0, 1, &clip);
        vkCmdDrawIndexed(recording.commands, draw.index_count, 1, draw.first_index, 0, 0);
    }
}
void OverlayPipeline::close() noexcept {
    if (!device_)
        return;
    for (auto pipeline : pipelines_)
        if (pipeline)
            vkDestroyPipeline(device_, pipeline, nullptr);
    if (pool_)
        vkDestroyDescriptorPool(device_, pool_, nullptr);
    if (sampler_)
        vkDestroySampler(device_, sampler_, nullptr);
    if (layout_)
        vkDestroyPipelineLayout(device_, layout_, nullptr);
    if (descriptor_layout_)
        vkDestroyDescriptorSetLayout(device_, descriptor_layout_, nullptr);
    device_ = VK_NULL_HANDLE;
}
OverlayPipeline::~OverlayPipeline() {
    if (device_)
        std::fputs("Vulkan overlay: completion unproven; native pipeline ownership retained\n",
                   stderr);
}
} // namespace ngen::render
