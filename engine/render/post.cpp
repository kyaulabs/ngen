#include "render/post.hpp"
#include <cstdio>
#include <span>
#include <stdexcept>
#include <string>
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
    VkShaderModule handle{};
    check(vkCreateShaderModule(device, &info, nullptr, &handle), "create embedded post module");
    return handle;
}
#endif
} // namespace
PostPipeline::PostPipeline(VkDevice device) : device_(device) {
#if !defined(NGEN_COMPILED_SHADERS)
    device_ = VK_NULL_HANDLE;
    throw std::runtime_error("post shaders unavailable; enable the offline shader build");
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
              "create post descriptor layout");
        const VkPushConstantRange push{VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                       sizeof(shader::PostConstants)};
        VkPipelineLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout.setLayoutCount = 1;
        layout.pSetLayouts = &descriptor_layout_;
        layout.pushConstantRangeCount = 1;
        layout.pPushConstantRanges = &push;
        check(vkCreatePipelineLayout(device_, &layout, nullptr, &layout_),
              "create post pipeline layout");
        const std::array<VkDescriptorPoolSize, 2> sizes{
            {{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 4}, {VK_DESCRIPTOR_TYPE_SAMPLER, 4}}};
        VkDescriptorPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.maxSets = 4;
        pool.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        pool.pPoolSizes = sizes.data();
        check(vkCreateDescriptorPool(device_, &pool, nullptr, &pool_),
              "create post descriptor pool");
        const std::array layouts{descriptor_layout_, descriptor_layout_, descriptor_layout_,
                                 descriptor_layout_};
        VkDescriptorSetAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate.descriptorPool = pool_;
        allocate.descriptorSetCount = 4;
        allocate.pSetLayouts = layouts.data();
        check(vkAllocateDescriptorSets(device_, &allocate, sets_.data()),
              "allocate post descriptors");
        VkSamplerCreateInfo sampler{};
        sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler.magFilter = VK_FILTER_LINEAR;
        sampler.minFilter = VK_FILTER_LINEAR;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.maxLod = 0;
        check(vkCreateSampler(device_, &sampler, nullptr, &sampler_), "create post sampler");
        modules[0] = module(device_, compiled::post_vertex);
        modules[1] = module(device_, compiled::post_fragment);
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
        VkPipelineVertexInputStateCreateInfo vertex{};
        vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        VkPipelineViewportStateCreateInfo viewport{};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport.viewportCount = 1;
        viewport.scissorCount = 1;
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
        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments = &color;
        const std::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = 2;
        dynamic.pDynamicStates = dynamic_states.data();
        for (std::size_t i = 0; i < pipelines_.size(); ++i) {
            constexpr std::array formats{VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB,
                                         VK_FORMAT_B8G8R8A8_SRGB};
            const auto format = formats[i];
            VkPipelineRenderingCreateInfo rendering{};
            rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
            rendering.colorAttachmentCount = 1;
            rendering.pColorAttachmentFormats = &format;
            VkGraphicsPipelineCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            info.pNext = &rendering;
            info.stageCount = 2;
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
                  "create post graphics pipeline");
        }
        for (auto handle : modules)
            vkDestroyShaderModule(device_, handle, nullptr);
    } catch (...) {
        for (auto handle : modules)
            if (handle)
                vkDestroyShaderModule(device_, handle, nullptr);
        close();
        throw;
    }
#endif
}
void PostPipeline::record(const PostDraw &draw) const {
    const VkDescriptorImageInfo image{VK_NULL_HANDLE, draw.source,
                                      VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    const VkDescriptorImageInfo sampler{sampler_, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
    std::array<VkWriteDescriptorSet, 2> writes{};
    for (std::size_t i = 0; i < writes.size(); ++i) {
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].dstSet = sets_[draw.frame];
        writes[i].dstBinding = static_cast<std::uint32_t>(i);
        writes[i].descriptorCount = 1;
        writes[i].descriptorType =
            i == 0 ? VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE : VK_DESCRIPTOR_TYPE_SAMPLER;
        writes[i].pImageInfo = i == 0 ? &image : &sampler;
    }
    vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(writes.size()), writes.data(), 0,
                           nullptr);
    vkCmdBindPipeline(draw.commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipelines_[draw.format == VK_FORMAT_R8G8B8A8_UNORM  ? 0U
                                 : draw.format == VK_FORMAT_R8G8B8A8_SRGB ? 1U
                                                                          : 2U]);
    vkCmdBindDescriptorSets(draw.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 1,
                            &sets_[draw.frame], 0, nullptr);
    const VkViewport viewport{
        0, 0, static_cast<float>(draw.extent.width), static_cast<float>(draw.extent.height), 0, 1};
    const VkRect2D scissor{{0, 0}, draw.extent};
    vkCmdSetViewport(draw.commands, 0, 1, &viewport);
    vkCmdSetScissor(draw.commands, 0, 1, &scissor);
    vkCmdPushConstants(draw.commands, layout_, VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                       sizeof(draw.constants), &draw.constants);
    vkCmdDraw(draw.commands, 3, 1, 0, 0);
}
void PostPipeline::close() noexcept {
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
    pipelines_ = {};
    sets_ = {};
    pool_ = VK_NULL_HANDLE;
    sampler_ = VK_NULL_HANDLE;
    layout_ = VK_NULL_HANDLE;
    descriptor_layout_ = VK_NULL_HANDLE;
    device_ = VK_NULL_HANDLE;
}
PostPipeline::~PostPipeline() {
    if (device_)
        std::fputs("Vulkan post: completion unproven; native pipeline ownership retained\n",
                   stderr);
}
} // namespace ngen::render
