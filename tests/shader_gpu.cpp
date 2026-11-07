#include "render/context.hpp"
#include "render/shader_contract.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <stdexcept>
#include <vector>
#include <vk_mem_alloc.h>

namespace {
using namespace ngen::render;
namespace shader = ngen::render::shader;
void require(bool condition, const char *message) {
    if (!condition)
        throw std::runtime_error(message);
}
void check(VkResult result, const char *operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
}
// This is a compiler/device test fixture, not a second runtime resource path.
// No Context Buffer/Image is submitted natively: the fixture owns all its VMA
// allocations and proves completion before CPU access, replacement or teardown.
class Fixture {
  public:
    struct Buffer {
        VkBuffer handle{};
        VmaAllocation allocation{};
        void *mapped{};
        VkDeviceSize size{};
    };
    struct Image {
        VkImage handle{};
        VkImageView view{};
        VmaAllocation allocation{};
        VkFormat format{};
        std::uint32_t layers{};
        VkExtent3D extent{};
        VkImageAspectFlags aspect{};
    };
    explicit Fixture(std::unique_ptr<Context> owner) : context(std::move(owner)) {
        buffers.reserve(32);
        images.reserve(16);
        pipelines.reserve(8);
        pipeline_layouts.reserve(8);
        set_layouts.reserve(8);
        samplers.reserve(4);
        shader_modules.reserve(8);
    }
    Fixture(const Fixture &) = delete;
    Fixture &operator=(const Fixture &) = delete;
    ~Fixture() { (void)close(); }
    void initialize() {
        std::uint32_t queue_count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(context->physical_device(), &queue_count, nullptr);
        require(queue_count > 0 && queue_count <= 64, "shader fixture queue query limit");
        std::array<VkQueueFamilyProperties, 64> families{};
        vkGetPhysicalDeviceQueueFamilyProperties(context->physical_device(), &queue_count,
                                                 families.data());
        require(context->queue_family() < queue_count &&
                    (families[context->queue_family()].queueFlags & VK_QUEUE_COMPUTE_BIT) != 0,
                "selected graphics queue cannot execute the culling shader fixture");
        VmaAllocatorCreateInfo info{};
        info.instance = context->instance();
        info.physicalDevice = context->physical_device();
        info.device = context->device();
        info.vulkanApiVersion = VK_API_VERSION_1_3;
        check(vmaCreateAllocator(&info, &allocator), "create shader fixture allocator");
        VkCommandPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        pool.queueFamilyIndex = context->queue_family();
        check(vkCreateCommandPool(device(), &pool, nullptr, &command_pool), "create shader pool");
        VkCommandBufferAllocateInfo command{};
        command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        command.commandPool = command_pool;
        command.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        command.commandBufferCount = 1;
        check(vkAllocateCommandBuffers(device(), &command, &commands), "allocate shader commands");
        VkFenceCreateInfo fence_info{};
        fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        check(vkCreateFence(device(), &fence_info, nullptr, &fence), "create shader fence");
        const std::array sizes{VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8},
                               VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4},
                               VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 16},
                               VkDescriptorPoolSize{VK_DESCRIPTOR_TYPE_SAMPLER, 7}};
        VkDescriptorPoolCreateInfo descriptors{};
        descriptors.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descriptors.maxSets = 8;
        descriptors.poolSizeCount = static_cast<std::uint32_t>(sizes.size());
        descriptors.pPoolSizes = sizes.data();
        check(vkCreateDescriptorPool(device(), &descriptors, nullptr, &descriptor_pool),
              "create shader descriptor pool");
    }
    bool close() noexcept {
        if (!context)
            return true;
        const auto result = vkDeviceWaitIdle(device());
        if (result != VK_SUCCESS && result != VK_ERROR_DEVICE_LOST) {
            if (context.release())
                std::fputs("shader fixture: completion unproven; native ownership retained\n",
                           stderr);
            return false;
        }
        for (auto pipeline : pipelines)
            vkDestroyPipeline(device(), pipeline, nullptr);
        for (auto module : shader_modules)
            vkDestroyShaderModule(device(), module, nullptr);
        for (auto layout : pipeline_layouts)
            vkDestroyPipelineLayout(device(), layout, nullptr);
        if (descriptor_pool)
            vkDestroyDescriptorPool(device(), descriptor_pool, nullptr);
        for (auto layout : set_layouts)
            vkDestroyDescriptorSetLayout(device(), layout, nullptr);
        for (auto sampler : samplers)
            vkDestroySampler(device(), sampler, nullptr);
        for (auto &image : images) {
            vkDestroyImageView(device(), image.view, nullptr);
            vmaDestroyImage(allocator, image.handle, image.allocation);
        }
        for (auto &buffer : buffers)
            vmaDestroyBuffer(allocator, buffer.handle, buffer.allocation);
        if (fence)
            vkDestroyFence(device(), fence, nullptr);
        if (command_pool)
            vkDestroyCommandPool(device(), command_pool, nullptr);
        if (allocator)
            vmaDestroyAllocator(allocator);
        const bool closed = context->close();
        const bool clean = context->validation_errors() == 0;
        context.reset();
        return closed && clean;
    }
    VkDevice device() const { return context->device(); }
    struct BufferInfo {
        VkDeviceSize size{};
        VkBufferUsageFlags usage{};
    };
    Buffer buffer(BufferInfo description) {
        require(buffers.size() < 32, "shader buffer fixture limit");
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = description.size;
        info.usage = description.usage;
        VmaAllocationCreateInfo allocation{};
        allocation.usage = VMA_MEMORY_USAGE_AUTO;
        allocation.flags =
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
        Buffer result{};
        VmaAllocationInfo details{};
        check(vmaCreateBuffer(allocator, &info, &allocation, &result.handle, &result.allocation,
                              &details),
              "create shader buffer");
        result.mapped = details.pMappedData;
        result.size = info.size;
        if (!result.mapped) {
            vmaDestroyBuffer(allocator, result.handle, result.allocation);
            throw std::runtime_error("shader fixture buffer was not mapped");
        }
        buffers.push_back(result);
        return result;
    }
    void write(Buffer target, std::span<const std::byte> bytes) {
        require(bytes.size() <= target.size, "shader fixture upload size");
        if (!target.mapped)
            throw std::runtime_error("shader fixture upload was not mapped");
        if (!bytes.empty()) {
            std::memcpy(target.mapped, bytes.data(), bytes.size());
            check(vmaFlushAllocation(allocator, target.allocation, 0, bytes.size()),
                  "flush shader upload");
        }
    }
    template <typename T> void write(Buffer target, const T &value) {
        write(target, std::as_bytes(std::span{&value, 1}));
    }
    void invalidate(Buffer target) {
        check(vmaInvalidateAllocation(allocator, target.allocation, 0, target.size),
              "invalidate shader readback");
    }
    void begin() {
        check(vkResetCommandPool(device(), command_pool, 0), "reset shader pool");
        VkCommandBufferBeginInfo begin_info{};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        check(vkBeginCommandBuffer(commands, &begin_info), "begin shader commands");
    }
    void submit() {
        check(vkEndCommandBuffer(commands), "end shader commands");
        check(vkResetFences(device(), 1, &fence), "reset shader fence");
        VkCommandBufferSubmitInfo command{};
        command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        command.commandBuffer = commands;
        VkSubmitInfo2 submit_info{};
        submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        submit_info.commandBufferInfoCount = 1;
        submit_info.pCommandBufferInfos = &command;
        check(vkQueueSubmit2(context->queue(), 1, &submit_info, fence), "submit shader commands");
        check(vkWaitForFences(device(), 1, &fence, VK_TRUE, 5'000'000'000ULL),
              "wait shader fixture");
    }
    struct MemoryBarrier {
        VkPipelineStageFlags2 srcStageMask{};
        VkAccessFlags2 srcAccessMask{};
        VkPipelineStageFlags2 dstStageMask{};
        VkAccessFlags2 dstAccessMask{};
    };
    void barrier(MemoryBarrier description) {
        VkMemoryBarrier2 memory{};
        memory.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        memory.srcStageMask = description.srcStageMask;
        memory.srcAccessMask = description.srcAccessMask;
        memory.dstStageMask = description.dstStageMask;
        memory.dstAccessMask = description.dstAccessMask;
        VkDependencyInfo dependency{};
        dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency.memoryBarrierCount = 1;
        dependency.pMemoryBarriers = &memory;
        vkCmdPipelineBarrier2(commands, &dependency);
    }
    VkShaderModule module(const std::filesystem::path &file) {
        require(shader_modules.size() < 8, "shader module fixture limit");
        std::ifstream input(file, std::ios::binary | std::ios::ate);
        require(input.good(), "shader module missing");
        const auto size = input.tellg();
        require(size >= 20 && size <= std::streamoff{4} * 1024 * 1024 && size % 4 == 0,
                "shader module size");
        std::vector<std::uint32_t> code(static_cast<std::size_t>(size) / 4);
        input.seekg(0);
        input.read(reinterpret_cast<char *>(code.data()), static_cast<std::streamsize>(size));
        require(input.good() && code[0] == 0x07230203 && code[1] == 0x00010500,
                "shader module header/read");
        VkShaderModuleCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        info.codeSize = static_cast<std::size_t>(size);
        info.pCode = code.data();
        VkShaderModule result{};
        check(vkCreateShaderModule(device(), &info, nullptr, &result), "create shader module");
        shader_modules.push_back(result);
        return result;
    }
    VkDescriptorSetLayout set_layout(std::span<const VkDescriptorSetLayoutBinding> bindings) {
        require(set_layouts.size() < 8, "shader set fixture limit");
        VkDescriptorSetLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.bindingCount = static_cast<std::uint32_t>(bindings.size());
        info.pBindings = bindings.data();
        VkDescriptorSetLayout result{};
        check(vkCreateDescriptorSetLayout(device(), &info, nullptr, &result),
              "create shader set layout");
        set_layouts.push_back(result);
        return result;
    }
    VkPipelineLayout pipeline_layout(std::span<const VkDescriptorSetLayout> layouts) {
        require(pipeline_layouts.size() < 8, "shader layout fixture limit");
        VkPipelineLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.setLayoutCount = static_cast<std::uint32_t>(layouts.size());
        info.pSetLayouts = layouts.data();
        VkPipelineLayout result{};
        check(vkCreatePipelineLayout(device(), &info, nullptr, &result),
              "create shader pipeline layout");
        pipeline_layouts.push_back(result);
        return result;
    }
    VkDescriptorSet descriptor_set(VkDescriptorSetLayout layout) {
        VkDescriptorSetAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        info.descriptorPool = descriptor_pool;
        info.descriptorSetCount = 1;
        info.pSetLayouts = &layout;
        VkDescriptorSet result{};
        check(vkAllocateDescriptorSets(device(), &info, &result), "allocate shader descriptor set");
        return result;
    }
    void descriptor(VkDescriptorSet set, std::uint32_t binding, VkDescriptorType type,
                    Buffer value) {
        const VkDescriptorBufferInfo buffer_info{value.handle, 0, value.size};
        VkWriteDescriptorSet write_info{};
        write_info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write_info.dstSet = set;
        write_info.dstBinding = binding;
        write_info.descriptorCount = 1;
        write_info.descriptorType = type;
        write_info.pBufferInfo = &buffer_info;
        vkUpdateDescriptorSets(device(), 1, &write_info, 0, nullptr);
    }
    Image image(VkFormat format, VkExtent3D extent, VkImageUsageFlags usage, bool cube = false) {
        require(images.size() < 16, "shader image fixture limit");
        VkImageCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags =
                cube ? static_cast<VkImageCreateFlags>(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT) : 0U,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = format,
            .extent = extent,
            .mipLevels = 1,
            .arrayLayers = cube ? 6U : 1U,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = usage,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
        VmaAllocationCreateInfo allocation{};
        allocation.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        Image result{};
        result.format = format;
        result.layers = info.arrayLayers;
        result.extent = extent;
        result.aspect =
            format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
        check(vmaCreateImage(allocator, &info, &allocation, &result.handle, &result.allocation,
                             nullptr),
              "create shader fixture image");
        VkImageViewCreateInfo view{};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = result.handle;
        view.viewType = cube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
        view.format = format;
        view.subresourceRange = {result.aspect, 0, 1, 0, result.layers};
        const auto status = vkCreateImageView(device(), &view, nullptr, &result.view);
        if (status != VK_SUCCESS) {
            vmaDestroyImage(allocator, result.handle, result.allocation);
            check(status, "create shader fixture view");
        }
        images.push_back(result);
        return result;
    }
    struct ImageBarrier {
        VkPipelineStageFlags2 srcStageMask{};
        VkAccessFlags2 srcAccessMask{};
        VkPipelineStageFlags2 dstStageMask{};
        VkAccessFlags2 dstAccessMask{};
        VkImageLayout oldLayout{};
        VkImageLayout newLayout{};
    };
    void image_barrier(Image target, ImageBarrier description) {
        VkImageMemoryBarrier2 memory{};
        memory.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        memory.srcStageMask = description.srcStageMask;
        memory.srcAccessMask = description.srcAccessMask;
        memory.dstStageMask = description.dstStageMask;
        memory.dstAccessMask = description.dstAccessMask;
        memory.oldLayout = description.oldLayout;
        memory.newLayout = description.newLayout;
        memory.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memory.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        memory.image = target.handle;
        memory.subresourceRange = {target.aspect, 0, 1, 0, target.layers};
        VkDependencyInfo dependency{};
        dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &memory;
        vkCmdPipelineBarrier2(commands, &dependency);
    }
    Image texture(VkFormat format, std::span<const std::byte> bytes, bool cube = false,
                  VkExtent2D extent = {1, 1}) {
        const auto staging =
            buffer({.size = bytes.size(), .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT});
        write(staging, bytes);
        auto result = image(format, {extent.width, extent.height, 1},
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, cube);
        begin();
        barrier({.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                 .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
                 .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                 .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT});
        image_barrier(result, {.srcStageMask = VK_PIPELINE_STAGE_2_NONE,
                               .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                               .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                               .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                               .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL});
        VkBufferImageCopy copy{};
        copy.imageSubresource = {result.aspect, 0, 0, result.layers};
        copy.imageExtent = {extent.width, extent.height, 1};
        vkCmdCopyBufferToImage(commands, staging.handle, result.handle,
                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);
        image_barrier(result, {.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                               .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                               .dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
                               .dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT,
                               .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               .newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});
        submit();
        return result;
    }
    Image constant_texture(shader::Float4 value, bool cube = false) {
        std::array<shader::Float4, 6> faces{};
        faces.fill(value);
        return texture(VK_FORMAT_R32G32B32A32_SFLOAT,
                       std::as_bytes(std::span{faces.data(), cube ? 6U : 1U}), cube);
    }
    enum class Wrap : std::uint8_t { clamp, repeat };
    VkSampler sampler(bool comparison, Wrap wrap = Wrap::clamp) {
        require(samplers.size() < 4, "shader sampler fixture limit");
        VkSamplerCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VK_FILTER_NEAREST;
        info.minFilter = VK_FILTER_NEAREST;
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        info.addressModeU = info.addressModeV = info.addressModeW =
            wrap == Wrap::repeat ? VK_SAMPLER_ADDRESS_MODE_REPEAT
                                 : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        info.compareEnable = comparison ? VK_TRUE : VK_FALSE;
        info.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        VkSampler result{};
        check(vkCreateSampler(device(), &info, nullptr, &result), "create shader sampler");
        samplers.push_back(result);
        return result;
    }
    void image_descriptor(VkDescriptorSet set, std::uint32_t binding, Image value) {
        const VkDescriptorImageInfo image_info{VK_NULL_HANDLE, value.view,
                                               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkWriteDescriptorSet info{};
        info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        info.dstSet = set;
        info.dstBinding = binding;
        info.descriptorCount = 1;
        info.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        info.pImageInfo = &image_info;
        vkUpdateDescriptorSets(device(), 1, &info, 0, nullptr);
    }
    void sampler_descriptors(VkDescriptorSet set, std::uint32_t binding,
                             std::span<const VkSampler> values) {
        require(!values.empty() && values.size() <= 5, "bounded sampler descriptor array");
        std::array<VkDescriptorImageInfo, 5> image_info{};
        for (std::size_t i = 0; i < values.size(); ++i)
            image_info[i] = {values[i], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_UNDEFINED};
        VkWriteDescriptorSet info{};
        info.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        info.dstSet = set;
        info.dstBinding = binding;
        info.descriptorCount = static_cast<std::uint32_t>(values.size());
        info.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
        info.pImageInfo = image_info.data();
        vkUpdateDescriptorSets(device(), 1, &info, 0, nullptr);
    }
    void sampler_descriptor(VkDescriptorSet set, std::uint32_t binding, VkSampler value) {
        sampler_descriptors(set, binding, std::span{&value, 1});
    }
    void compute(const std::filesystem::path &package) {
        const auto empty = set_layout({});
        const std::array bindings{
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                         VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                         VK_SHADER_STAGE_COMPUTE_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                         VK_SHADER_STAGE_COMPUTE_BIT, nullptr}};
        const auto layout = set_layout(bindings);
        const std::array layouts{empty, empty, layout};
        const auto pipeline_layout_handle = pipeline_layout(layouts);
        const auto shader_module = module(package / "cull.spv");
        VkComputePipelineCreateInfo info{
            .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = {.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                      .pNext = nullptr,
                      .flags = 0,
                      .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                      .module = VK_NULL_HANDLE,
                      .pName = nullptr,
                      .pSpecializationInfo = nullptr},
            .layout = VK_NULL_HANDLE,
            .basePipelineHandle = VK_NULL_HANDLE,
            .basePipelineIndex = -1};
        info.stage.module = shader_module;
        info.stage.pName = "main";
        info.layout = pipeline_layout_handle;
        VkPipeline pipeline{};
        const auto result =
            vkCreateComputePipelines(device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
        if (pipeline)
            pipelines.push_back(pipeline);
        check(result, "create culling pipeline");
        shader::CullConstants constants{};
        constants.planes = {{{1, 0, 0, 1},
                             {-1, 0, 0, 1},
                             {0, 1, 0, 1},
                             {0, -1, 0, 1},
                             {0, 0, 1, 1},
                             {0, 0, -1, 1}}};
        std::array<shader::Float4, 192> spheres{};
        for (std::size_t i = 0; i < spheres.size(); ++i) {
            const auto axis = (i / 3) % 3;
            spheres[i][3] = 0.25F;
            spheres[i][axis] = std::array{0.0F, 1.25F, -1.5F}[i % 3];
        }
        const auto uniform =
            buffer({.size = sizeof(constants), .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT});
        const auto input =
            buffer({.size = sizeof(spheres), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT});
        std::array<std::uint32_t, 192> values{};
        const auto output =
            buffer({.size = sizeof(values), .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT});
        write(input, spheres);
        const auto set = descriptor_set(layout);
        descriptor(set, 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, uniform);
        descriptor(set, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, input);
        descriptor(set, 2, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, output);
        for (auto count : {0U, 1U, 63U, 64U, 65U, 130U}) {
            constants.count = count;
            write(uniform, constants);
            values.fill(0xdeadbeef);
            write(output, values);
            begin();
            barrier({.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                     .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
                     .dstStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                     .dstAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT |
                                      VK_ACCESS_2_SHADER_STORAGE_READ_BIT |
                                      VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT});
            vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
            vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_COMPUTE,
                                    pipeline_layout_handle, 2, 1, &set, 0, nullptr);
            vkCmdDispatch(commands, 3, 1, 1);
            barrier({.srcStageMask = VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT,
                     .srcAccessMask = VK_ACCESS_2_SHADER_STORAGE_WRITE_BIT,
                     .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                     .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT});
            submit();
            invalidate(output);
            std::memcpy(values.data(), output.mapped, sizeof(values));
            for (std::size_t i = 0; i < values.size(); ++i) {
                const auto expected = i < count ? (i % 3 == 2 ? 0U : 1U) : 0xdeadbeefU;
                require(values[i] == expected, "culling output or out-of-range guard mismatch");
            }
        }
        std::cout << "compute_counts=6 compute_values=1152 cull_boundary_and_guard=1\n";
    }
    void graphics(const std::filesystem::path &package, std::ostream *capture) {
        constexpr std::uint32_t dimension = 64;
        const std::array frame_bindings{
            VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                         VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
                                         nullptr},
            VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                         VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            VkDescriptorSetLayoutBinding{2, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1,
                                         VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            VkDescriptorSetLayoutBinding{3, VK_DESCRIPTOR_TYPE_SAMPLER, 1,
                                         VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
            VkDescriptorSetLayoutBinding{4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                         VK_SHADER_STAGE_VERTEX_BIT, nullptr}};
        std::array<VkDescriptorSetLayoutBinding, 11> material_bindings{};
        for (std::uint32_t i = 0; i < material_bindings.size(); ++i)
            material_bindings[i] = {i,
                                    i == 0 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                           : (i == 6 || i == 10 ? VK_DESCRIPTOR_TYPE_SAMPLER
                                                                : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE),
                                    i == 6 ? 5U : 1U, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        const std::array layouts{set_layout(frame_bindings), set_layout(material_bindings)};
        const auto layout = pipeline_layout(layouts);
        const std::array sets{descriptor_set(layouts[0]), descriptor_set(layouts[1])};
        std::array stages{VkPipelineShaderStageCreateInfo{
                              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                              .pNext = nullptr,
                              .flags = 0,
                              .stage = VK_SHADER_STAGE_VERTEX_BIT,
                              .module = VK_NULL_HANDLE,
                              .pName = nullptr,
                              .pSpecializationInfo = nullptr},
                          VkPipelineShaderStageCreateInfo{
                              .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                              .pNext = nullptr,
                              .flags = 0,
                              .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
                              .module = VK_NULL_HANDLE,
                              .pName = nullptr,
                              .pSpecializationInfo = nullptr}};
        for (std::size_t i = 0; i < stages.size(); ++i) {
            stages[i].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            stages[i].stage = i == 0 ? VK_SHADER_STAGE_VERTEX_BIT : VK_SHADER_STAGE_FRAGMENT_BIT;
            stages[i].module = module(package / (i == 0 ? "pbr-vertex.spv" : "pbr-fragment.spv"));
            stages[i].pName = "main";
        }
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
        VkPipelineVertexInputStateCreateInfo vertex_info{};
        vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertex_info.vertexBindingDescriptionCount = 1;
        vertex_info.pVertexBindingDescriptions = &vertex_binding;
        vertex_info.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributes.size());
        vertex_info.pVertexAttributeDescriptions = attributes.data();
        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        const VkViewport viewport{
            0, 0, static_cast<float>(dimension), static_cast<float>(dimension), 0, 1};
        const VkRect2D scissor{{0, 0}, {dimension, dimension}};
        VkPipelineViewportStateCreateInfo viewport_info{};
        viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewport_info.viewportCount = viewport_info.scissorCount = 1;
        viewport_info.pViewports = &viewport;
        viewport_info.pScissors = &scissor;
        VkPipelineRasterizationStateCreateInfo raster{};
        raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        raster.polygonMode = VK_POLYGON_MODE_FILL;
        raster.cullMode = VK_CULL_MODE_NONE;
        raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        raster.lineWidth = 1;
        VkPipelineMultisampleStateCreateInfo multisample{
            .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
            .sampleShadingEnable = VK_FALSE,
            .minSampleShading = 0,
            .pSampleMask = nullptr,
            .alphaToCoverageEnable = VK_FALSE,
            .alphaToOneEnable = VK_FALSE};
        VkPipelineColorBlendAttachmentState attachment{};
        attachment.colorWriteMask = static_cast<VkColorComponentFlags>(VK_COLOR_COMPONENT_R_BIT) |
                                    VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                                    VK_COLOR_COMPONENT_A_BIT;
        VkPipelineColorBlendStateCreateInfo blend{};
        blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blend.attachmentCount = 1;
        blend.pAttachments = &attachment;
        const VkFormat output_format = VK_FORMAT_R32G32B32A32_SFLOAT;
        VkPipelineRenderingCreateInfo rendering{};
        rendering.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        rendering.colorAttachmentCount = 1;
        rendering.pColorAttachmentFormats = &output_format;
        VkGraphicsPipelineCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        info.pNext = &rendering;
        info.stageCount = static_cast<std::uint32_t>(stages.size());
        info.pStages = stages.data();
        info.pVertexInputState = &vertex_info;
        info.pInputAssemblyState = &assembly;
        info.pViewportState = &viewport_info;
        info.pRasterizationState = &raster;
        info.pMultisampleState = &multisample;
        info.pColorBlendState = &blend;
        info.layout = layout;
        VkPipeline pipeline{};
        const auto result =
            vkCreateGraphicsPipelines(device(), VK_NULL_HANDLE, 1, &info, nullptr, &pipeline);
        if (pipeline)
            pipelines.push_back(pipeline);
        check(result, "create PBR pipeline");
        shader::FrameConstants frame{};
        const shader::Matrix identity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
        frame.viewProjection = frame.normalMatrix = frame.lightViewProjection = identity;
        frame.model = {{0.5F, 0, 0, 0, 0, 0.75F, 0, 0, 0, 0, 1, 0, 0.25F, -0.125F, 0.2F, 1}};
        frame.cameraPosition = {0.265625F, -0.109375F, 3.2F, 0};
        frame.keyDirectionIntensity = {0, 0, -1, 0};
        frame.keyColor = {1, 1, 1, 0};
        shader::LightConstants lights{};
        shader::MaterialConstants material{};
        material.emissiveAlphaCutoff = {0.25F, 0.5F, 0.75F, 0.5F};
        const auto frame_buffer =
            buffer({.size = sizeof(frame), .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT});
        const auto light_buffer =
            buffer({.size = sizeof(lights), .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT});
        const auto material_buffer =
            buffer({.size = sizeof(material), .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT});
        descriptor(sets[0], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, frame_buffer);
        descriptor(sets[0], 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, light_buffer);
        descriptor(sets[1], 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, material_buffer);
        shader::InstanceConstants instance_constants{};
        instance_constants.transforms[0] = {identity, identity};
        const auto instance_buffer = buffer(
            {.size = sizeof(instance_constants), .usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT});
        write(instance_buffer, instance_constants);
        descriptor(sets[0], 4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, instance_buffer);
        const std::array<std::uint8_t, 4> color_bytes{128, 64, 32, 255};
        const auto srgb = texture(VK_FORMAT_R8G8B8A8_SRGB, std::as_bytes(std::span{color_bytes}));
        const auto linear =
            texture(VK_FORMAT_R8G8B8A8_UNORM, std::as_bytes(std::span{color_bytes}));
        const auto normal = constant_texture({0.5F, 0.5F, 1, 1});
        const auto reverse_normal = constant_texture({0.5F, 0.5F, 0, 1});
        const auto mr = constant_texture({0, 0.5F, 0.25F, 1});
        const auto occlusion = constant_texture({0.4F, 0, 0, 1});
        const auto emissive = constant_texture({1, 1, 1, 1});
        const auto irradiance = constant_texture({0.1F, 0.15F, 0.2F, 1}, true);
        const auto prefiltered = constant_texture({0.2F, 0.25F, 0.3F, 1}, true);
        const auto brdf = constant_texture({0.3F, 0.1F, 0, 1});
        const float shadow_depth = 1;
        const auto shadow =
            texture(VK_FORMAT_D32_SFLOAT, std::as_bytes(std::span{&shadow_depth, 1}));
        const float blocked_depth = 0;
        const auto blocked_shadow =
            texture(VK_FORMAT_D32_SFLOAT, std::as_bytes(std::span{&blocked_depth, 1}));
        image_descriptor(sets[0], 2, shadow);
        sampler_descriptor(sets[0], 3, sampler(true));
        image_descriptor(sets[1], 1, srgb);
        image_descriptor(sets[1], 2, normal);
        image_descriptor(sets[1], 3, mr);
        image_descriptor(sets[1], 4, occlusion);
        image_descriptor(sets[1], 5, emissive);
        const auto material_sampler = sampler(false);
        const std::array material_samplers{material_sampler, material_sampler, material_sampler,
                                           material_sampler, material_sampler};
        sampler_descriptors(sets[1], 6, material_samplers);
        sampler_descriptor(sets[1], 10, sampler(false));
        image_descriptor(sets[1], 7, irradiance);
        image_descriptor(sets[1], 8, prefiltered);
        image_descriptor(sets[1], 9, brdf);
        std::array<shader::Vertex, 6> vertices{};
        const std::array<std::array<float, 2>, 6> positions{{{-0.6F, -0.6F},
                                                             {0.6F, -0.6F},
                                                             {-0.6F, 0.6F},
                                                             {-0.6F, 0.6F},
                                                             {0.6F, -0.6F},
                                                             {0.6F, 0.6F}}};
        for (std::size_t i = 0; i < vertices.size(); ++i) {
            vertices[i].position = {positions[i][0], positions[i][1], 0};
            vertices[i].normal = {0, 0, 1};
            vertices[i].uv = {0.75F, 0.75F};
            vertices[i].uv1 = {0.25F, 0.25F};
            vertices[i].tangent = {1, 0, 0, 1};
        }
        const auto vertex_buffer =
            buffer({.size = sizeof(vertices), .usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT});
        write(vertex_buffer, vertices);
        const auto target =
            image(output_format, {dimension, dimension, 1},
                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT);
        const auto readback =
            buffer({.size = VkDeviceSize{dimension} * dimension * sizeof(shader::Float4),
                    .usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT});
        bool initialized = false;
        const auto draw = [&] {
            write(frame_buffer, frame);
            write(light_buffer, lights);
            write(material_buffer, material);
            begin();
            barrier({.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                     .srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT,
                     .dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
                     .dstAccessMask =
                         VK_ACCESS_2_UNIFORM_READ_BIT | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT});
            image_barrier(target, {.srcStageMask = initialized ? VK_PIPELINE_STAGE_2_COPY_BIT
                                                               : VK_PIPELINE_STAGE_2_NONE,
                                   .srcAccessMask = initialized ? VK_ACCESS_2_TRANSFER_READ_BIT : 0,
                                   .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                   .oldLayout = initialized ? VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
                                                            : VK_IMAGE_LAYOUT_UNDEFINED,
                                   .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
            VkRenderingAttachmentInfo color{};
            color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
            color.imageView = target.view;
            color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color.clearValue.color = {{0.03125F, 0.0625F, 0.125F, 0}};
            VkRenderingInfo pass{};
            pass.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
            pass.renderArea = scissor;
            pass.layerCount = 1;
            pass.colorAttachmentCount = 1;
            pass.pColorAttachments = &color;
            vkCmdBeginRendering(commands, &pass);
            vkCmdBindPipeline(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
            vkCmdBindDescriptorSets(commands, VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0,
                                    static_cast<std::uint32_t>(sets.size()), sets.data(), 0,
                                    nullptr);
            const VkDeviceSize offset = 0;
            vkCmdBindVertexBuffers(commands, 0, 1, &vertex_buffer.handle, &offset);
            vkCmdDraw(commands, 6, 1, 0, 0);
            vkCmdEndRendering(commands);
            image_barrier(target, {.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                                   .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                                   .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                                   .dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT,
                                   .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                                   .newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL});
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {dimension, dimension, 1};
            vkCmdCopyImageToBuffer(commands, target.handle, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   readback.handle, 1, &copy);
            barrier({.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                     .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                     .dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT,
                     .dstAccessMask = VK_ACCESS_2_HOST_READ_BIT});
            submit();
            initialized = true;
            invalidate(readback);
        };
        const auto pixel = [&](std::size_t x, std::size_t y) {
            shader::Float4 value{};
            std::memcpy(value.data(),
                        static_cast<const std::byte *>(readback.mapped) +
                            (y * dimension + x) * sizeof(value),
                        sizeof(value));
            return value;
        };
        const auto equal_pixel = [](shader::Float4 actual, shader::Float4 expected,
                                    float tolerance) {
            for (std::size_t channel = 0; channel < 4; ++channel) {
                if (!std::isfinite(actual[channel]) ||
                    std::abs(actual[channel] - expected[channel]) > tolerance) {
                    std::cerr << "pixel channel=" << channel << " actual=" << actual[channel]
                              << " expected=" << expected[channel] << '\n';
                    throw std::runtime_error("PBR pixel mismatch");
                }
            }
        };
        // Non-symmetric, non-uniform model transform proves CPU matrix convention.
        // Alpha distinguishes mesh coverage independently of the material RGB.
        draw();
        for (std::size_t y = 0; y < dimension; ++y)
            for (std::size_t x = 0; x < dimension; ++x) {
                const auto nx = (static_cast<float>(x) + 0.5F) / 32 - 1;
                const auto ny = (static_cast<float>(y) + 0.5F) / 32 - 1;
                const bool inside = nx > -0.05F && nx < 0.55F && ny > -0.575F && ny < 0.325F;
                if (inside) {
                    const auto actual = pixel(x, y);
                    require(std::all_of(actual.begin(), actual.end(),
                                        [](float channel) { return std::isfinite(channel); }),
                            "nonfinite PBR output");
                    require(actual[3] == 1, "matrix-transformed geometry coverage mismatch");
                } else
                    equal_pixel(pixel(x, y), {0.03125F, 0.0625F, 0.125F, 0}, 0.00001F);
            }
        // Isolate direct metallic-roughness, constant IBL, occlusion and color decode.
        material.baseColor = {0.8F, 0.6F, 0.4F, 1};
        material.emissiveAlphaCutoff = {0, 0, 0, 0.5F};
        material.metallicRoughnessNormalOcclusion[3] = 0.5F;
        frame.keyDirectionIntensity[3] = 1;
        float direct_scale = 1;
        const auto reference = [&](bool decode_srgb) {
            const float roughness = 0.5F * material.metallicRoughnessNormalOcclusion[1];
            const float metallic = 0.25F * material.metallicRoughnessNormalOcclusion[0];
            shader::Float4 expected{0, 0, 0, 1};
            constexpr float pi = 3.14159265358979323846F;
            for (std::size_t c = 0; c < 3; ++c) {
                float albedo = static_cast<float>(color_bytes[c]) / 255;
                if (decode_srgb)
                    albedo = albedo <= 0.04045F ? albedo / 12.92F
                                                : std::pow((albedo + 0.055F) / 1.055F, 2.4F);
                albedo *= material.baseColor[c];
                const auto f0 = 0.04F * (1 - metallic) + albedo * metallic;
                const auto diffuse = (1 - f0) * (1 - metallic) * albedo / pi;
                const auto specular = f0 / (4 * pi * roughness * roughness * roughness * roughness);
                const auto irradiance_value = std::array{0.1F, 0.15F, 0.2F}[c];
                const auto prefiltered_value = std::array{0.2F, 0.25F, 0.3F}[c];
                expected[c] =
                    (diffuse + specular) * direct_scale +
                    (diffuse * irradiance_value + prefiltered_value * (f0 * 0.3F + 0.1F)) * 0.7F;
            }
            return expected;
        };
        draw();
        equal_pixel(pixel(40, 28), reference(true), 0.004F);
        if (capture) {
            *capture << "PF\n64 64\n-1.0\n";
            for (std::size_t y = dimension; y-- > 0;)
                for (std::size_t x = 0; x < dimension; ++x) {
                    const auto actual = pixel(x, y);
                    capture->write(reinterpret_cast<const char *>(actual.data()),
                                   3 * sizeof(float));
                }
            capture->flush();
            require(capture->good(), "write HDR fixture capture");
        }
        material.emissiveAlphaCutoff = {0.25F, 0.5F, 0.75F, 0.5F};
        draw();
        auto emissive_reference = reference(true);
        for (std::size_t c = 0; c < 3; ++c)
            emissive_reference[c] += material.emissiveAlphaCutoff[c];
        equal_pixel(pixel(40, 28), emissive_reference, 0.004F);
        material.emissiveAlphaCutoff = {0, 0, 0, 0.5F};
        image_descriptor(sets[0], 2, blocked_shadow);
        draw();
        direct_scale = 0;
        equal_pixel(pixel(40, 28), reference(true), 0.004F);
        direct_scale = 1;
        image_descriptor(sets[0], 2, shadow);
        image_descriptor(sets[1], 1, linear);
        draw();
        equal_pixel(pixel(40, 28), reference(false), 0.004F);
        image_descriptor(sets[1], 1, srgb);
        material.metallicRoughnessNormalOcclusion[1] = 1.6F;
        material.metallicRoughnessNormalOcclusion[0] = 0;
        draw();
        equal_pixel(pixel(40, 28), reference(true), 0.004F);
        frame.keyDirectionIntensity[3] = 0;
        lights.count = 17; // The shader must clamp to the actual 16-element uniform array.
        lights.lights[0].positionRange = {frame.cameraPosition[0], frame.cameraPosition[1], 2.2F,
                                          0};
        lights.lights[0].colorIntensity = {1, 1, 1, 4};
        draw();
        equal_pixel(pixel(40, 28), reference(true), 0.004F);
        lights = {};
        const std::array<shader::Float4, 4> uv_colors{
            {{1, 0, 0, 1}, {0, 1, 0, 1}, {0, 0, 1, 1}, {1, 1, 0, 1}}};
        const auto uv_texture = texture(VK_FORMAT_R32G32B32A32_SFLOAT,
                                        std::as_bytes(std::span(uv_colors)), false, {2, 2});
        image_descriptor(sets[1], 5, uv_texture);
        material.emissiveAlphaCutoff = {1, 1, 1, .5F};
        draw();
        const auto uv0_pixel = pixel(40, 28);
        material.flags[1] = 1U << 4U;
        draw();
        const auto uv1_pixel = pixel(40, 28);
        equal_pixel({uv0_pixel[0] - uv1_pixel[0], uv0_pixel[1] - uv1_pixel[1],
                     uv0_pixel[2] - uv1_pixel[2], uv0_pixel[3] - uv1_pixel[3]},
                    {0, 1, 0, 0}, .004F);
        for (auto &vertex : vertices)
            vertex.uv1 = {1.25F, .25F};
        write(vertex_buffer, vertices);
        draw();
        const auto clamped_uv = pixel(40, 28);
        auto per_map_samplers = material_samplers;
        per_map_samplers[4] = sampler(false, Wrap::repeat);
        sampler_descriptors(sets[1], 6, per_map_samplers);
        draw();
        const auto repeated_uv = pixel(40, 28);
        equal_pixel({repeated_uv[0] - clamped_uv[0], repeated_uv[1] - clamped_uv[1],
                     repeated_uv[2] - clamped_uv[2], repeated_uv[3] - clamped_uv[3]},
                    {1, -1, 0, 0}, .004F);
        sampler_descriptors(sets[1], 6, material_samplers);
        for (auto &vertex : vertices)
            vertex.uv1 = {.25F, .25F};
        write(vertex_buffer, vertices);
        material.flags[1] = 0;
        material.emissiveAlphaCutoff = {0, 0, 0, .5F};
        image_descriptor(sets[1], 5, emissive);
        image_descriptor(sets[1], 2, reverse_normal);
        draw();
        equal_pixel(pixel(40, 28), {0.056F, 0.07F, 0.084F, 1}, 0.004F);
        material.flags[2] = 1;
        frame.keyDirectionIntensity[3] = 1;
        draw();
        equal_pixel(pixel(40, 28), reference(true), .004F);
        material.flags[2] = 3;
        draw();
        const auto two_sided_a = pixel(40, 28);
        std::swap(vertices[1], vertices[2]);
        std::swap(vertices[4], vertices[5]);
        write(vertex_buffer, vertices);
        draw();
        const auto two_sided_b = pixel(40, 28);
        const shader::Float4 back_reference{.056F, .07F, .084F, 1};
        if (std::abs(two_sided_a[0] - reference(true)[0]) < .004F) {
            equal_pixel(two_sided_a, reference(true), .004F);
            equal_pixel(two_sided_b, back_reference, .004F);
        } else {
            equal_pixel(two_sided_a, back_reference, .004F);
            equal_pixel(two_sided_b, reference(true), .004F);
        }
        std::swap(vertices[1], vertices[2]);
        std::swap(vertices[4], vertices[5]);
        write(vertex_buffer, vertices);
        material.flags[2] = 0;
        frame.keyDirectionIntensity[3] = 0;
        material.flags[0] = 1;
        material.baseColor[3] = 0.25F;
        draw();
        equal_pixel(pixel(40, 28), {0.03125F, 0.0625F, 0.125F, 0}, 0.00001F);
        std::cout << "pbr_draws=16 two_sided_normal=1 uv1_selection=1 per_map_sampler=1 "
                     "absent_normal_map=1 "
                     "matrix_coverage_pixels=4096 "
                     "srgb_linear=1 "
                     "metallic_roughness=1 "
                     "local_light_clamp=1 normal_map=1 alpha_mask=1 emissive=1 shadow_compare=1\n";
    }
    std::unique_ptr<Context> context;
    VkCommandBuffer commands{};
    std::vector<VkPipeline> pipelines;

  private:
    VmaAllocator allocator{};
    VkCommandPool command_pool{};
    VkFence fence{};
    VkDescriptorPool descriptor_pool{};
    std::vector<Buffer> buffers;
    std::vector<Image> images;
    std::vector<VkPipelineLayout> pipeline_layouts;
    std::vector<VkDescriptorSetLayout> set_layouts;
    std::vector<VkSampler> samplers;
    std::vector<VkShaderModule> shader_modules;
};
} // namespace

int main(int argc, char **argv) {
    try {
        require(argc <= 3, "usage: shader_gpu_probe [cooked shader directory] [capture.pfm]");
        const std::filesystem::path package = argc > 1 ? argv[1] : NGEN_SHADER_PACKAGE_DIR;
        std::ofstream capture;
        if (argc > 2) {
            const std::filesystem::path path{argv[2]};
            require(!std::filesystem::exists(path) && !std::filesystem::is_symlink(path),
                    "capture already exists");
            capture.open(path, std::ios::binary);
            require(capture.good(), "open HDR fixture capture");
        }
        // Opt into Context's existing explicit synchronization-validation contract;
        // this fixture itself renders offscreen and never requests a presented frame.
        auto created =
            Context::create({.validation = true, .visible = false, .presentation = true});
        if (const auto *failure = std::get_if<Failure>(&created))
            throw std::runtime_error(failure->message);
        Fixture fixture{std::move(std::get<std::unique_ptr<Context>>(created))};
        fixture.initialize();
        fixture.compute(package);
        fixture.graphics(package, argc > 2 ? &capture : nullptr);
        require(fixture.context->validation_errors() == 0,
                "unexpected shader validation diagnostic");
        require(fixture.close(), "shader fixture shutdown failed");
        std::cout << "shader_gpu=1 validation_clean=1\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "shader GPU probe: " << error.what() << '\n';
        return 1;
    }
}
