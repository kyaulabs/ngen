#include "render/mesh_internal.hpp"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#if defined(NGEN_COMPILED_SHADERS)
#include "ngen_shader_code.hpp"
#endif

namespace ngen::render {
namespace {
void check(VkResult result, const char *operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
}
#if defined(NGEN_COMPILED_SHADERS)
VkShaderModule module(VkDevice device, std::span<const std::uint32_t> words) {
    VkShaderModuleCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    info.codeSize = words.size_bytes();
    info.pCode = words.data();
    VkShaderModule result{};
    check(vkCreateShaderModule(device, &info, nullptr, &result), "create embedded mesh module");
    return result;
}
#endif
} // namespace
MeshPipeline::MeshPipeline(VkDevice device, VkPhysicalDevice physical) : device_(device) {
#if !defined(NGEN_COMPILED_SHADERS)
    (void)physical;
    device_ = VK_NULL_HANDLE;
    throw std::runtime_error("mesh shaders unavailable; enable the offline shader build");
#else
    std::array<VkShaderModule, 4> modules{};
    try {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(physical, &properties);
        const auto alignment =
            std::max(VkDeviceSize{1}, properties.limits.minUniformBufferOffsetAlignment);
        if (alignment > 4096)
            throw std::runtime_error("mesh uniform alignment exceeds the fixed profile");
        const auto aligned = [alignment](VkDeviceSize size) {
            return (size + alignment - 1) / alignment * alignment;
        };
        frame_stride_ = aligned(sizeof(shader::FrameConstants));
        light_offset_ = frame_stride_ * draw_limit;
        material_stride_ = aligned(sizeof(shader::MaterialConstants));
        material_offset_ = light_offset_ + aligned(sizeof(shader::LightConstants));
        scratch_.resize(static_cast<std::size_t>(material_offset_ + material_stride_ * draw_limit));
        const std::array frame_bindings{
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                         nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                         VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1,
                                         VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_SAMPLER, 1,
                                         VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}};
        std::array<VkDescriptorSetLayoutBinding, 11> material_bindings{};
        for (std::uint32_t i = 0; i < material_bindings.size(); ++i)
            material_bindings[i] = {i,
                                    i == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                           : (i == 6 || i == 10 ? VK_DESCRIPTOR_TYPE_SAMPLER
                                                                : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE),
                                    i == 6 ? 5U : 1U, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        const std::array<std::span<const VkDescriptorSetLayoutBinding>, 2> bindings{
            frame_bindings, material_bindings};
        for (std::size_t i = 0; i < bindings.size(); ++i) {
            VkDescriptorSetLayoutCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            info.bindingCount = static_cast<std::uint32_t>(bindings[i].size());
            info.pBindings = bindings[i].data();
            check(vkCreateDescriptorSetLayout(device_, &info, nullptr, &descriptor_layouts_[i]),
                  "create mesh descriptor layout");
        }
        VkPipelineLayoutCreateInfo layout{};
        layout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout.setLayoutCount = 2;
        layout.pSetLayouts = descriptor_layouts_.data();
        check(vkCreatePipelineLayout(device_, &layout, nullptr, &layout_),
              "create mesh pipeline layout");
        constexpr auto records = static_cast<std::uint32_t>(2 * draw_limit);
        const std::array sizes{VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, records * 3},
                               VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, records * 9},
                               VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, records * 7}};
        VkDescriptorPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool.maxSets = records * 2;
        pool.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        pool.pPoolSizes = sizes.data();
        check(vkCreateDescriptorPool(device_, &pool, nullptr, &pool_),
              "create mesh descriptor pool");
        std::array<VkDescriptorSetLayout, 2 * draw_limit * 2> layouts{};
        for (std::size_t i = 0; i < layouts.size(); ++i)
            layouts[i] = descriptor_layouts_[i % 2];
        VkDescriptorSetAllocateInfo allocate{};
        allocate.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocate.descriptorPool = pool_;
        allocate.descriptorSetCount = static_cast<std::uint32_t>(sets_.size());
        allocate.pSetLayouts = layouts.data();
        check(vkAllocateDescriptorSets(device_, &allocate, sets_.data()),
              "allocate mesh descriptor slots");
        VkSamplerCreateInfo sampler_info{};
        sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        sampler_info.magFilter = VK_FILTER_NEAREST;
        sampler_info.minFilter = VK_FILTER_NEAREST;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        sampler_info.addressModeU = sampler_info.addressModeV = sampler_info.addressModeW =
            VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler_info.compareEnable = VK_TRUE;
        sampler_info.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        check(vkCreateSampler(device_, &sampler_info, nullptr, &shadow_sampler_),
              "create mesh shadow sampler");
        sampler_info.compareEnable = VK_FALSE;
        sampler_info.magFilter = VK_FILTER_LINEAR;
        sampler_info.minFilter = VK_FILTER_LINEAR;
        sampler_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler_info.maxLod = VK_LOD_CLAMP_NONE;
        check(vkCreateSampler(device_, &sampler_info, nullptr, &environment_sampler_),
              "create mesh environment sampler");
        modules[0] = module(device_, compiled::pbr_vertex);
        modules[1] = module(device_, compiled::pbr_fragment);
        modules[2] = module(device_, compiled::shadow_vertex);
        modules[3] = module(device_, compiled::shadow_fragment);
        std::array stages{VkPipelineShaderStageCreateInfo{
                              VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                              VK_SHADER_STAGE_VERTEX_BIT, modules[0], "main", nullptr},
                          VkPipelineShaderStageCreateInfo{
                              VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
                              VK_SHADER_STAGE_FRAGMENT_BIT, modules[1], "main", nullptr}};
        const VkVertexInputBindingDescription vertex_binding{0, sizeof(shader::Vertex),
                                                             VK_VERTEX_INPUT_RATE_VERTEX};
        const std::array attributes{
            VkVertexInputAttributeDescription{0, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                              offsetof(shader::Vertex, position)},
            VkVertexInputAttributeDescription{1, 0, VK_FORMAT_R32G32B32_SFLOAT,
                                              offsetof(shader::Vertex, normal)},
            VkVertexInputAttributeDescription{2, 0, VK_FORMAT_R32G32_SFLOAT,
                                              offsetof(shader::Vertex, uv)},
            VkVertexInputAttributeDescription{3, 0, VK_FORMAT_R32G32B32A32_SFLOAT,
                                              offsetof(shader::Vertex, tangent)},
            VkVertexInputAttributeDescription{4, 0, VK_FORMAT_R32G32_SFLOAT,
                                              offsetof(shader::Vertex, uv1)}};
        const std::array shadow_attributes{attributes[0], attributes[2], attributes[4]};
        VkPipelineVertexInputStateCreateInfo vertex{};
        vertex.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex.vertexBindingDescriptionCount = 1;
        vertex.pVertexBindingDescriptions = &vertex_binding;
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
        raster.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo multisample{
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            nullptr,
            0,
            VK_SAMPLE_COUNT_1_BIT,
            VK_FALSE,
            0,
            nullptr,
            VK_FALSE,
            VK_FALSE};
        VkPipelineDepthStencilStateCreateInfo depth{};
        depth.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depth.depthTestEnable = VK_TRUE;
        depth.depthCompareOp = VK_COMPARE_OP_LESS;
        VkPipelineColorBlendAttachmentState attachment{};
        attachment.colorWriteMask = static_cast<VkColorComponentFlags>(VK_COLOR_COMPONENT_R_BIT) |
                                    VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                    VK_COLOR_COMPONENT_A_BIT;
        attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.colorBlendOp = VK_BLEND_OP_ADD;
        attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        attachment.alphaBlendOp = VK_BLEND_OP_ADD;
        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments = &attachment;
        const std::array dynamic_states{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo dynamic{};
        dynamic.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamic.dynamicStateCount = static_cast<std::uint32_t>(dynamic_states.size());
        dynamic.pDynamicStates = dynamic_states.data();
        const VkFormat color_format = VK_FORMAT_R16G16B16A16_SFLOAT;
        VkPipelineRenderingCreateInfo rendering{};
        rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachmentFormats = &color_format;
        rendering.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT;
        VkGraphicsPipelineCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.pNext = &rendering;
        info.stageCount = 2;
        info.pStages = stages.data();
        info.pVertexInputState = &vertex;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pDepthStencilState = &depth;
        info.pColorBlendState = &blend;
        info.pDynamicState = &dynamic;
        info.layout = layout_;
        for (std::size_t i = 0; i < pipelines_.size(); ++i) {
            const auto facing = i % 3;
            const bool shadow = i >= 6, transparent = i >= 3 && !shadow;
            stages[0].module = modules[shadow ? 2 : 0];
            stages[1].module = modules[shadow ? 3 : 1];
            vertex.vertexAttributeDescriptionCount =
                static_cast<std::uint32_t>(shadow ? shadow_attributes.size() : attributes.size());
            vertex.pVertexAttributeDescriptions =
                shadow ? shadow_attributes.data() : attributes.data();
            rendering.colorAttachmentCount = blend.attachmentCount = shadow ? 0U : 1U;
            rendering.pColorAttachmentFormats = shadow ? nullptr : &color_format;
            raster.cullMode = facing == 0 ? VK_CULL_MODE_NONE : VK_CULL_MODE_BACK_BIT;
            raster.frontFace =
                facing == 1 ? VK_FRONT_FACE_CLOCKWISE : VK_FRONT_FACE_COUNTER_CLOCKWISE;
            depth.depthWriteEnable = transparent ? VK_FALSE : VK_TRUE;
            attachment.blendEnable = transparent ? VK_TRUE : VK_FALSE;
            check(vkCreateGraphicsPipelines(device_, VK_NULL_HANDLE, 1, &info, nullptr,
                                            &pipelines_[i]),
                  "create prewarmed mesh pipeline");
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
MeshPipeline::~MeshPipeline() {
    if (device_)
        std::fputs("Vulkan mesh: completion unproven; native pipeline ownership retained\n",
                   stderr);
}
void MeshPipeline::close() noexcept {
    if (!device_)
        return;
    for (auto pipeline : pipelines_)
        if (pipeline)
            vkDestroyPipeline(device_, pipeline, nullptr);
    if (pool_)
        vkDestroyDescriptorPool(device_, pool_, nullptr);
    if (layout_)
        vkDestroyPipelineLayout(device_, layout_, nullptr);
    for (auto layout : descriptor_layouts_)
        if (layout)
            vkDestroyDescriptorSetLayout(device_, layout, nullptr);
    for (std::size_t i = 0; i < sampler_count_; ++i)
        vkDestroySampler(device_, samplers_[i].native, nullptr);
    if (shadow_sampler_)
        vkDestroySampler(device_, shadow_sampler_, nullptr);
    if (environment_sampler_)
        vkDestroySampler(device_, environment_sampler_, nullptr);
    device_ = VK_NULL_HANDLE;
}
VkSampler MeshPipeline::sampler(const MeshSampler &description) {
    for (std::size_t i = 0; i < sampler_count_; ++i)
        if (samplers_[i].description == description)
            return samplers_[i].native;
    if (sampler_count_ == samplers_.size())
        throw std::runtime_error("mesh sampler cache is full");
    VkSamplerCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    info.magFilter = description.mag;
    info.minFilter = description.min;
    info.mipmapMode = description.mip;
    info.addressModeU = description.u;
    info.addressModeV = description.v;
    info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    info.maxLod = description.mipmaps ? VK_LOD_CLAMP_NONE : 0;
    VkSampler native{};
    check(vkCreateSampler(device_, &info, nullptr, &native), "create bounded mesh sampler");
    samplers_[sampler_count_++] = {description, native};
    return native;
}
std::span<const std::byte> MeshPipeline::prepare(const MeshPass &pass) {
    std::memcpy(scratch_.data() + light_offset_, &pass.lights, sizeof(pass.lights));
    for (std::size_t i = 0; i < pass.draws.size(); ++i) {
        std::memcpy(scratch_.data() + i * frame_stride_, &pass.draws[i].frame,
                    sizeof(shader::FrameConstants));
        std::memcpy(scratch_.data() + material_offset_ + i * material_stride_,
                    &pass.draws[i].material, sizeof(shader::MaterialConstants));
    }
    return scratch_;
}
void MeshPipeline::descriptors(const MeshRecording &recording) {
    for (std::size_t i = 0; i < recording.draws.size(); ++i) {
        const auto &draw = recording.draws[i];
        const auto set_index = (recording.slot * draw_limit + i) * 2;
        const std::array sets{sets_[set_index], sets_[set_index + 1]};
        const std::array buffers{VkDescriptorBufferInfo{recording.uniform, i * frame_stride_,
                                                        sizeof(shader::FrameConstants)},
                                 VkDescriptorBufferInfo{recording.uniform, light_offset_,
                                                        sizeof(shader::LightConstants)},
                                 VkDescriptorBufferInfo{recording.uniform,
                                                        material_offset_ + i * material_stride_,
                                                        sizeof(shader::MaterialConstants)}};
        std::array<VkDescriptorImageInfo, 9> images{};
        images[0] = {VK_NULL_HANDLE, recording.shadow, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        constexpr std::array<std::size_t, 5> slots{0, 2, 1, 3, 4};
        for (std::size_t map = 0; map < slots.size(); ++map)
            images[map + 1] = {VK_NULL_HANDLE, recording.bindings[i].maps[slots[map]],
                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        images[6] = {VK_NULL_HANDLE, recording.irradiance,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        images[7] = {VK_NULL_HANDLE, recording.prefiltered,
                     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        images[8] = {VK_NULL_HANDLE, recording.brdf, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        std::array<VkDescriptorImageInfo, 7> samplers{};
        samplers[0].sampler = shadow_sampler_;
        samplers[6].sampler = environment_sampler_;
        for (std::size_t map = 0; map < 5; ++map)
            samplers[map + 1].sampler = sampler(draw.samplers[map]);
        std::array<VkWriteDescriptorSet, 15> writes{};
        std::size_t count = 0;
        const auto add = [&](VkDescriptorSet set, std::uint32_t binding, VkDescriptorType type,
                             std::uint32_t size, const VkDescriptorBufferInfo *buffer,
                             const VkDescriptorImageInfo *image) {
            auto &write = writes[count++];
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = set;
            write.dstBinding = binding;
            write.descriptorType = type;
            write.descriptorCount = size;
            write.pBufferInfo = buffer;
            write.pImageInfo = image;
        };
        add(sets[0], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &buffers[0], nullptr);
        add(sets[0], 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &buffers[1], nullptr);
        add(sets[0], 2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, nullptr, &images[0]);
        add(sets[0], 3, VK_DESCRIPTOR_TYPE_SAMPLER, 1, nullptr, &samplers[0]);
        add(sets[1], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, &buffers[2], nullptr);
        for (std::uint32_t map = 0; map < 5; ++map)
            add(sets[1], map + 1, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, nullptr, &images[map + 1]);
        add(sets[1], 6, VK_DESCRIPTOR_TYPE_SAMPLER, 5, nullptr, &samplers[1]);
        for (std::uint32_t env = 0; env < 3; ++env)
            add(sets[1], 7 + env, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1, nullptr, &images[6 + env]);
        add(sets[1], 10, VK_DESCRIPTOR_TYPE_SAMPLER, 1, nullptr, &samplers[6]);
        vkUpdateDescriptorSets(device_, static_cast<std::uint32_t>(count), writes.data(), 0,
                               nullptr);
    }
}
void MeshPipeline::record(const MeshRecording &recording) {
    const VkViewport viewport{0,
                              0,
                              static_cast<float>(recording.extent.width),
                              static_cast<float>(recording.extent.height),
                              0,
                              1};
    const VkRect2D scissor{{0, 0}, recording.extent};
    vkCmdSetViewport(recording.commands, 0, 1, &viewport);
    vkCmdSetScissor(recording.commands, 0, 1, &scissor);
    const VkDeviceSize offset = 0;
    vkCmdBindVertexBuffers(recording.commands, 0, 1, &recording.vertices, &offset);
    vkCmdBindIndexBuffer(recording.commands, recording.indices, 0, VK_INDEX_TYPE_UINT32);
    for (std::size_t i = 0; i < recording.draws.size(); ++i) {
        const auto &draw = recording.draws[i];
        if (recording.shadow_pass && draw.material.flags[0] == 2)
            continue;
        const auto set_index = (recording.slot * draw_limit + i) * 2;
        const std::array sets{sets_[set_index], sets_[set_index + 1]};
        const auto pipeline =
            static_cast<std::size_t>(draw.facing) +
            (recording.shadow_pass ? 6U : (draw.material.flags[0] == 2 ? 3U : 0U));
        vkCmdBindPipeline(recording.commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipelines_[pipeline]);
        vkCmdBindDescriptorSets(recording.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout_, 0, 2,
                                sets.data(), 0, nullptr);
        vkCmdDrawIndexed(recording.commands, draw.index_count, 1, draw.first_index,
                         draw.vertex_offset, 0);
    }
}
} // namespace ngen::render
