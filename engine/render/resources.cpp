#include "render/mesh_internal.hpp"
#include "render/post.hpp"
#include "render/resources_internal.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vk_mem_alloc.h>

namespace ngen::render {
namespace {
void resource_check(VkResult result, const char *operation) {
    if (result != VK_SUCCESS)
        throw std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result));
}
} // namespace
struct Resources::Impl {
    static constexpr std::size_t capacity = resource_record_limit;
    static constexpr VkDeviceSize budget = resource_allocation_limit;
    struct Entry {
        VkBuffer buffer{};
        VkImage image{};
        VkImageView view{};
        VmaAllocation allocation{};
        VkDeviceSize bytes{};
        VkDeviceSize size{};
        VkBufferUsageFlags usage{};
        VkImageUsageFlags image_usage{};
        VkExtent2D image_extent{};
        std::uint32_t mip_levels{1};
        std::uint32_t texel_bytes{4};
        std::uint32_t layers{1};
        VkImageAspectFlags aspect{VK_IMAGE_ASPECT_COLOR_BIT};
        VkFormat format{};
        bool linear_filterable{};
        VkImageLayout image_layout{VK_IMAGE_LAYOUT_UNDEFINED};
        MemoryAccess memory{MemoryAccess::device};
        std::uint64_t last_use{};
        std::uint32_t generation{1};
        bool retired{};
    };
    struct Frame {
        VkCommandPool pool{};
        VkCommandBuffer commands{};
        std::uint64_t submitted{};
    };
    std::unique_ptr<PostPipeline> post_pipeline;
    std::unique_ptr<MeshPipeline> mesh_pipeline;
    std::array<Buffer, 2> mesh_uniforms;
    VmaAllocator allocator{};
    VkDevice device{};
    VkPhysicalDevice physical{};
    std::uint32_t max_image_dimension{};
    std::uint32_t max_cube_dimension{};
    VkQueue queue{};
    VkSemaphore timeline{};
    std::array<Frame, 2> frames{};
    std::array<Entry, capacity> entries{};
    VkDeviceSize allocated{};
    std::uint64_t submitted{};
    std::uint64_t host_acquired{};
    std::uint64_t external_wait_submission{};
    [[nodiscard]] const Entry *find(detail::ResourceKey key) const noexcept {
        if (!allocator || key.slot >= entries.size())
            return nullptr;
        const auto &entry = entries[key.slot];
        return entry.generation == key.generation && !entry.retired && (entry.buffer || entry.image)
                   ? &entry
                   : nullptr;
    }
    void destroy(Entry &entry) noexcept {
        if (!entry.buffer && !entry.image)
            return;
        if (entry.view)
            vkDestroyImageView(device, entry.view, nullptr);
        if (entry.image)
            vmaDestroyImage(allocator, entry.image, entry.allocation);
        else
            vmaDestroyBuffer(allocator, entry.buffer, entry.allocation);
        allocated -= entry.bytes;
        const auto next = entry.generation == UINT32_MAX ? 0U : entry.generation + 1U;
        entry = {};
        entry.generation = next;
    }
    std::uint64_t collect() {
        std::uint64_t complete = 0;
        resource_check(vkGetSemaphoreCounterValue(device, timeline, &complete),
                       "query resource completion");
        if (complete > host_acquired) {
            VkSemaphoreWaitInfo wait{};
            wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            wait.semaphoreCount = 1;
            wait.pSemaphores = &timeline;
            wait.pValues = &complete;
            resource_check(vkWaitSemaphores(device, &wait, 0), "acquire completed resources");
            host_acquired = complete;
        }
        for (auto &entry : entries)
            if (entry.retired && entry.last_use <= complete)
                destroy(entry);
        return complete;
    }
    std::variant<Frame *, Failure> begin(TimelineWait wait) {
        if (!wait.semaphore && wait.value != 0)
            return Failure{"timeline wait has no semaphore"};
        if (submitted == UINT64_MAX)
            return Failure{"resource submission counter exhausted"};
        const auto complete = collect();
        const auto available =
            std::find_if(frames.begin(), frames.end(),
                         [complete](const auto &frame) { return frame.submitted <= complete; });
        if (available == frames.end())
            return Failure{"transfer contexts are busy"};
        auto &frame = *available;
        resource_check(vkResetCommandPool(device, frame.pool, 0), "reset transfer pool");
        VkCommandBufferBeginInfo info{};
        info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        resource_check(vkBeginCommandBuffer(frame.commands, &info), "begin transfer");
        VkMemoryBarrier2 memory{};
        memory.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        memory.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        memory.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        memory.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        memory.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        VkDependencyInfo dependency{};
        dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency.memoryBarrierCount = 1;
        dependency.pMemoryBarriers = &memory;
        vkCmdPipelineBarrier2(frame.commands, &dependency);
        return &frame;
    }
    static void host_read(Frame &frame) {
        VkMemoryBarrier2 memory{};
        memory.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
        memory.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
        memory.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
        memory.dstStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
        memory.dstAccessMask = VK_ACCESS_2_HOST_READ_BIT;
        VkDependencyInfo dependency{};
        dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency.memoryBarrierCount = 1;
        dependency.pMemoryBarriers = &memory;
        vkCmdPipelineBarrier2(frame.commands, &dependency);
    }
    static std::array<VkBufferImageCopy, 32> image_regions(const Entry &entry) {
        std::array<VkBufferImageCopy, 32> regions{};
        auto width = entry.image_extent.width;
        auto height = entry.image_extent.height;
        VkDeviceSize offset = 0;
        for (std::uint32_t level = 0; level < entry.mip_levels; ++level) {
            regions[level].bufferOffset = offset;
            regions[level].imageSubresource = {entry.aspect, level, 0, entry.layers};
            regions[level].imageExtent = {width, height, 1};
            offset += static_cast<VkDeviceSize>(width) * height * entry.texel_bytes * entry.layers;
            width = std::max(1U, width / 2);
            height = std::max(1U, height / 2);
        }
        return regions;
    }
    static void image_barrier(Frame &frame, VkImageMemoryBarrier2 barrier, std::uint32_t levels) {
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange = {barrier.subresourceRange.aspectMask, 0, levels, 0,
                                    barrier.subresourceRange.layerCount};
        VkDependencyInfo dependency{};
        dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
        dependency.imageMemoryBarrierCount = 1;
        dependency.pImageMemoryBarriers = &barrier;
        vkCmdPipelineBarrier2(frame.commands, &dependency);
    }
    std::uint64_t submit(Frame &frame, TimelineWait dependency) {
        resource_check(vkEndCommandBuffer(frame.commands), "end transfer");
        VkCommandBufferSubmitInfo command{};
        command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
        command.commandBuffer = frame.commands;
        command.deviceMask = 1;
        const auto next = submitted + 1;
        VkSemaphoreSubmitInfo signal{};
        signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        signal.semaphore = timeline;
        signal.value = next;
        signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        VkSemaphoreSubmitInfo wait{};
        wait.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
        wait.semaphore = dependency.semaphore;
        wait.value = dependency.value;
        wait.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        VkSubmitInfo2 info{};
        info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
        info.waitSemaphoreInfoCount = dependency.semaphore ? 1U : 0U;
        info.pWaitSemaphoreInfos = dependency.semaphore ? &wait : nullptr;
        info.commandBufferInfoCount = 1;
        info.pCommandBufferInfos = &command;
        info.signalSemaphoreInfoCount = 1;
        info.pSignalSemaphoreInfos = &signal;
        resource_check(vkQueueSubmit2(queue, 1, &info, VK_NULL_HANDLE), "submit transfer");
        frame.submitted = next;
        submitted = next;
        if (dependency.semaphore)
            external_wait_submission = next;
        return next;
    }
};
Resources::Resources(const Context &owner) : impl_(std::make_unique<Impl>()) {
    impl_->device = owner.device();
    impl_->physical = owner.physical_device();
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(impl_->physical, &properties);
    impl_->max_image_dimension = properties.limits.maxImageDimension2D;
    impl_->max_cube_dimension = properties.limits.maxImageDimensionCube;
    impl_->queue = owner.queue();
    try {
        VmaAllocatorCreateInfo info{};
        info.instance = owner.instance();
        info.physicalDevice = owner.physical_device();
        info.device = owner.device();
        info.vulkanApiVersion = VK_API_VERSION_1_3;
        info.preferredLargeHeapBlockSize = 8ULL * 1024 * 1024;
        resource_check(vmaCreateAllocator(&info, &impl_->allocator), "create VMA allocator");
        VkSemaphoreTypeCreateInfo type{};
        type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo semaphore{};
        semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        semaphore.pNext = &type;
        resource_check(vkCreateSemaphore(impl_->device, &semaphore, nullptr, &impl_->timeline),
                       "create resource timeline");
        for (auto &frame : impl_->frames) {
            VkCommandPoolCreateInfo pool{};
            pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
            pool.queueFamilyIndex = owner.queue_family();
            resource_check(vkCreateCommandPool(impl_->device, &pool, nullptr, &frame.pool),
                           "create transfer pool");
            VkCommandBufferAllocateInfo commands{};
            commands.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            commands.commandPool = frame.pool;
            commands.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            commands.commandBufferCount = 1;
            resource_check(vkAllocateCommandBuffers(impl_->device, &commands, &frame.commands),
                           "allocate transfer commands");
        }
    } catch (...) {
        close(); // Construction cannot have submitted work.
        throw;
    }
}
Resources::~Resources() {
    if (impl_->allocator)
        std::fputs("Vulkan resources: completion unproven; native allocations retained\n", stderr);
}
std::variant<Buffer, Failure> Resources::create(BufferDescription description) {
    constexpr VkBufferUsageFlags allowed =
        static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_TRANSFER_SRC_BIT) |
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT |
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    if (!impl_->allocator)
        return Failure{"resource allocator is closed"};
    (void)impl_->collect();
    if (description.bytes == 0 || description.bytes > Impl::budget - impl_->allocated)
        return Failure{"buffer size exceeds the resource admission budget"};
    if (description.usage == 0 || (description.usage & ~allowed) != 0)
        return Failure{"unsupported buffer usage"};
    if (description.memory != MemoryAccess::device && description.memory != MemoryAccess::upload &&
        description.memory != MemoryAccess::readback)
        return Failure{"unsupported buffer memory access"};
    const auto free =
        std::find_if(impl_->entries.begin(), impl_->entries.end(), [](const auto &entry) {
            return !entry.buffer && !entry.image && entry.generation != 0;
        });
    if (free == impl_->entries.end())
        return Failure{"resource slot capacity exhausted"};
    VkBufferCreateInfo buffer{};
    buffer.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer.size = description.bytes;
    buffer.usage = description.usage;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkDeviceBufferMemoryRequirements query{};
    query.sType = VK_STRUCTURE_TYPE_DEVICE_BUFFER_MEMORY_REQUIREMENTS;
    query.pCreateInfo = &buffer;
    VkMemoryRequirements2 requirements{};
    requirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vkGetDeviceBufferMemoryRequirements(impl_->device, &query, &requirements);
    if (requirements.memoryRequirements.size > Impl::budget - impl_->allocated)
        return Failure{"buffer requirements exceed the resource admission budget"};
    VmaAllocationCreateInfo allocation{};
    allocation.usage = description.memory == MemoryAccess::device
                           ? VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE
                           : VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    if (description.memory != MemoryAccess::device) {
        allocation.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT |
                           (description.memory == MemoryAccess::upload
                                ? VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT
                                : VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT);
    }
    VkBuffer handle{};
    VmaAllocation memory{};
    VmaAllocationInfo details{};
    const auto result =
        vmaCreateBuffer(impl_->allocator, &buffer, &allocation, &handle, &memory, &details);
    if (result != VK_SUCCESS)
        return Failure{"create buffer: VkResult " + std::to_string(result)};
    if (details.size > Impl::budget - impl_->allocated) {
        vmaDestroyBuffer(impl_->allocator, handle, memory);
        return Failure{"aligned buffer size exceeds the resource admission budget"};
    }
    free->buffer = handle;
    free->allocation = memory;
    free->bytes = details.size;
    free->size = description.bytes;
    free->usage = description.usage;
    free->memory = description.memory;
    impl_->allocated += details.size;
    const auto slot = static_cast<std::uint32_t>(free - impl_->entries.begin());
    return Buffer{weak_from_this(), {slot, free->generation}};
}
std::variant<Image, Failure> Resources::create(ImageDescription description) {
    if (!impl_->allocator)
        return Failure{"resource allocator is closed"};
    (void)impl_->collect();
    if (description.extent.width == 0 || description.extent.height == 0 ||
        description.extent.width > impl_->max_image_dimension ||
        description.extent.height > impl_->max_image_dimension)
        return Failure{"invalid image extent"};
    if (description.format != VK_FORMAT_R8G8B8A8_SRGB &&
        description.format != VK_FORMAT_R8G8B8A8_UNORM &&
        description.format != VK_FORMAT_R16G16B16A16_SFLOAT &&
        description.format != VK_FORMAT_D32_SFLOAT)
        return Failure{"unsupported image format"};
    const std::uint32_t texel_bytes = description.format == VK_FORMAT_R16G16B16A16_SFLOAT ? 8U : 4U;
    const bool depth = description.format == VK_FORMAT_D32_SFLOAT;
    const auto layers = description.cube ? 6U : 1U;
    const VkImageCreateFlags flags =
        description.cube ? static_cast<VkImageCreateFlags>(VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT)
                         : 0U;
    if (description.cube && (depth || description.extent.width != description.extent.height ||
                             description.extent.width > impl_->max_cube_dimension))
        return Failure{"invalid color cubemap extent or format"};
    const auto aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
    if ((description.usage & (depth ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                    : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)) != 0)
        return Failure{"image format and attachment usage are incompatible"};
    constexpr VkImageUsageFlags allowed =
        static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT) |
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    constexpr VkImageUsageFlags view_usage =
        static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_SAMPLED_BIT) |
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if ((description.usage & view_usage) == 0 || (description.usage & ~allowed) != 0)
        return Failure{"image usage must support an owned view"};
    if (description.mip_levels == 0 ||
        description.mip_levels > static_cast<std::uint32_t>(std::bit_width(std::max(
                                     description.extent.width, description.extent.height))))
        return Failure{"invalid image mip count"};
    const auto available = Impl::budget - impl_->allocated;
    VkDeviceSize image_bytes = 0;
    auto width = description.extent.width;
    auto height = description.extent.height;
    for (std::uint32_t level = 0; level < description.mip_levels; ++level) {
        const auto remaining = available - image_bytes;
        const auto row_bytes = static_cast<VkDeviceSize>(width) * texel_bytes * layers;
        if (row_bytes > remaining || height > remaining / row_bytes)
            return Failure{"image dimensions exceed the resource admission budget"};
        image_bytes += row_bytes * height;
        width = std::max(1U, width / 2);
        height = std::max(1U, height / 2);
    }
    VkFormatFeatureFlags required{};
    if ((description.usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) != 0)
        required |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    if ((description.usage & VK_IMAGE_USAGE_TRANSFER_DST_BIT) != 0)
        required |= VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if ((description.usage & VK_IMAGE_USAGE_SAMPLED_BIT) != 0)
        required |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if ((description.usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) != 0)
        required |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    if ((description.usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        required |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    VkFormatProperties properties{};
    vkGetPhysicalDeviceFormatProperties(impl_->physical, description.format, &properties);
    if ((properties.optimalTilingFeatures & required) != required)
        return Failure{"image format lacks required optimal-tiling features"};
    VkImageFormatProperties format_limits{};
    const auto format_result = vkGetPhysicalDeviceImageFormatProperties(
        impl_->physical, description.format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
        description.usage, flags, &format_limits);
    if (format_result != VK_SUCCESS)
        return Failure{"query image format: VkResult " + std::to_string(format_result)};
    if (description.extent.width > format_limits.maxExtent.width ||
        description.extent.height > format_limits.maxExtent.height ||
        format_limits.maxExtent.depth == 0 || format_limits.maxMipLevels < description.mip_levels ||
        format_limits.maxArrayLayers < layers ||
        (format_limits.sampleCounts & VK_SAMPLE_COUNT_1_BIT) == 0)
        return Failure{"image extent or samples exceed format limits"};
    if (image_bytes > format_limits.maxResourceSize)
        return Failure{"image pixels exceed format resource limit"};
    const auto free =
        std::find_if(impl_->entries.begin(), impl_->entries.end(), [](const auto &entry) {
            return !entry.buffer && !entry.image && entry.generation != 0;
        });
    if (free == impl_->entries.end())
        return Failure{"resource slot capacity exhausted"};
    VkImageCreateInfo info{.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                           .pNext = nullptr,
                           .flags = flags,
                           .imageType = VK_IMAGE_TYPE_2D,
                           .format = description.format,
                           .extent = {description.extent.width, description.extent.height, 1},
                           .mipLevels = description.mip_levels,
                           .arrayLayers = layers,
                           .samples = VK_SAMPLE_COUNT_1_BIT,
                           .tiling = VK_IMAGE_TILING_OPTIMAL,
                           .usage = description.usage,
                           .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
                           .queueFamilyIndexCount = 0,
                           .pQueueFamilyIndices = nullptr,
                           .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED};
    VkDeviceImageMemoryRequirements query{};
    query.sType = VK_STRUCTURE_TYPE_DEVICE_IMAGE_MEMORY_REQUIREMENTS;
    query.pCreateInfo = &info;
    VkMemoryRequirements2 requirements{};
    requirements.sType = VK_STRUCTURE_TYPE_MEMORY_REQUIREMENTS_2;
    vkGetDeviceImageMemoryRequirements(impl_->device, &query, &requirements);
    if (requirements.memoryRequirements.size > available)
        return Failure{"image requirements exceed the resource admission budget"};
    if (requirements.memoryRequirements.size > format_limits.maxResourceSize)
        return Failure{"image memory exceeds format resource limit"};
    VmaAllocationCreateInfo allocation{};
    allocation.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    VkImage handle{};
    VmaAllocation memory{};
    VmaAllocationInfo details{};
    const auto result =
        vmaCreateImage(impl_->allocator, &info, &allocation, &handle, &memory, &details);
    if (result != VK_SUCCESS)
        return Failure{"create image: VkResult " + std::to_string(result)};
    if (details.size > available) {
        vmaDestroyImage(impl_->allocator, handle, memory);
        return Failure{"aligned image size exceeds the resource admission budget"};
    }
    VkImageViewCreateInfo view_info{};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.image = handle;
    view_info.viewType = description.cube ? VK_IMAGE_VIEW_TYPE_CUBE : VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = description.format;
    view_info.subresourceRange = {aspect, 0, description.mip_levels, 0, layers};
    VkImageView view{};
    const auto view_result = vkCreateImageView(impl_->device, &view_info, nullptr, &view);
    if (view_result != VK_SUCCESS) {
        vmaDestroyImage(impl_->allocator, handle, memory);
        return Failure{"create image view: VkResult " + std::to_string(view_result)};
    }
    free->image = handle;
    free->image_usage = description.usage;
    free->image_extent = description.extent;
    free->mip_levels = description.mip_levels;
    free->texel_bytes = texel_bytes;
    free->layers = layers;
    free->aspect = aspect;
    free->format = description.format;
    free->linear_filterable =
        (properties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT) != 0;
    free->size = image_bytes;
    free->view = view;
    free->allocation = memory;
    free->bytes = details.size;
    impl_->allocated += details.size;
    const auto slot = static_cast<std::uint32_t>(free - impl_->entries.begin());
    return Image{weak_from_this(), {slot, free->generation}};
}
VkImage Resources::image_native(detail::ResourceKey key) const noexcept {
    const auto *entry = impl_->find(key);
    return entry ? entry->image : VK_NULL_HANDLE;
}
VkImageView Resources::image_view(detail::ResourceKey key) const noexcept {
    const auto *entry = impl_->find(key);
    return entry ? entry->view : VK_NULL_HANDLE;
}
VkBuffer Resources::native(detail::ResourceKey key) const noexcept {
    const auto *entry = impl_->find(key);
    return entry ? entry->buffer : VK_NULL_HANDLE;
}
std::variant<Submission, Failure> Resources::copy(BufferCopy description) {
    if (description.source.owner_.lock().get() != this ||
        description.destination.owner_.lock().get() != this)
        return Failure{"copy resources belong to another context"};
    const auto source_key = description.source.key_;
    const auto target_key = description.destination.key_;
    if (!native(source_key) || !native(target_key) || source_key.slot == target_key.slot)
        return Failure{"copy requires distinct live buffers"};
    auto &source = impl_->entries[source_key.slot];
    auto &target = impl_->entries[target_key.slot];
    if (description.bytes == 0 || description.bytes > source.size ||
        description.bytes > target.size)
        return Failure{"copy range exceeds buffer size"};
    if ((source.usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0 ||
        (target.usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0)
        return Failure{"copy buffer usage is incompatible"};
    auto recording = impl_->begin(description.wait);
    if (const auto *failure = std::get_if<Failure>(&recording))
        return *failure;
    auto &frame = *std::get<Impl::Frame *>(recording);
    const VkBufferCopy region{0, 0, description.bytes};
    vkCmdCopyBuffer(frame.commands, source.buffer, target.buffer, 1, &region);
    Impl::host_read(frame);
    const auto next = impl_->submit(frame, description.wait);
    source.last_use = next;
    target.last_use = next;
    return Submission{weak_from_this(), next};
}
std::variant<Submission, Failure> Resources::copy(ImageUpload description) {
    if (description.source.owner_.lock().get() != this ||
        description.destination.owner_.lock().get() != this)
        return Failure{"image upload resources belong to another context"};
    const auto source_key = description.source.key_;
    const auto target_key = description.destination.key_;
    if (!native(source_key) || !image_native(target_key))
        return Failure{"image upload requires live resources"};
    auto &source = impl_->entries[source_key.slot];
    auto &target = impl_->entries[target_key.slot];
    constexpr VkImageUsageFlags required =
        static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_DST_BIT) |
        VK_IMAGE_USAGE_SAMPLED_BIT;
    if ((source.usage & VK_BUFFER_USAGE_TRANSFER_SRC_BIT) == 0 ||
        (target.image_usage & required) != required)
        return Failure{"image upload usage is incompatible"};
    if (source.size < target.size)
        return Failure{"image upload buffer is too small"};
    auto recording = impl_->begin(description.wait);
    if (const auto *failure = std::get_if<Failure>(&recording))
        return *failure;
    auto &frame = *std::get<Impl::Frame *>(recording);
    VkImageMemoryBarrier2 barrier{};
    barrier.image = target.image;
    barrier.subresourceRange.aspectMask = target.aspect;
    barrier.subresourceRange.layerCount = target.layers;
    barrier.oldLayout = target.image_layout;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.srcStageMask = target.image_layout == VK_IMAGE_LAYOUT_UNDEFINED
                               ? VK_PIPELINE_STAGE_2_NONE
                               : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.srcAccessMask = target.image_layout == VK_IMAGE_LAYOUT_UNDEFINED
                                ? VK_ACCESS_2_NONE
                                : VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    Impl::image_barrier(frame, barrier, target.mip_levels);
    const auto regions = Impl::image_regions(target);
    vkCmdCopyBufferToImage(frame.commands, source.buffer, target.image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, target.mip_levels, regions.data());
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    Impl::image_barrier(frame, barrier, target.mip_levels);
    const auto next = impl_->submit(frame, description.wait);
    source.last_use = next;
    target.last_use = next;
    target.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return Submission{weak_from_this(), next};
}
std::variant<Submission, Failure> Resources::copy(ImageReadback description) {
    if (description.source.owner_.lock().get() != this ||
        description.destination.owner_.lock().get() != this)
        return Failure{"image readback resources belong to another context"};
    const auto source_key = description.source.key_;
    const auto target_key = description.destination.key_;
    if (!image_native(source_key) || !native(target_key))
        return Failure{"image readback requires live resources"};
    auto &source = impl_->entries[source_key.slot];
    auto &target = impl_->entries[target_key.slot];
    if ((source.image_usage & VK_IMAGE_USAGE_TRANSFER_SRC_BIT) == 0 ||
        (target.usage & VK_BUFFER_USAGE_TRANSFER_DST_BIT) == 0)
        return Failure{"image readback usage is incompatible"};
    if (source.image_layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        return Failure{"image has not been initialized for sampled use"};
    if (target.size < source.size)
        return Failure{"image readback buffer is too small"};
    auto recording = impl_->begin(description.wait);
    if (const auto *failure = std::get_if<Failure>(&recording))
        return *failure;
    auto &frame = *std::get<Impl::Frame *>(recording);
    VkImageMemoryBarrier2 barrier{};
    barrier.image = source.image;
    barrier.subresourceRange.aspectMask = source.aspect;
    barrier.subresourceRange.layerCount = source.layers;
    barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    Impl::image_barrier(frame, barrier, source.mip_levels);
    const auto regions = Impl::image_regions(source);
    vkCmdCopyImageToBuffer(frame.commands, source.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           target.buffer, source.mip_levels, regions.data());
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    Impl::image_barrier(frame, barrier, source.mip_levels);
    Impl::host_read(frame);
    const auto next = impl_->submit(frame, description.wait);
    source.last_use = next;
    target.last_use = next;
    return Submission{weak_from_this(), next};
}
std::variant<Submission, Failure> Resources::post(PostProcess description) {
    if (description.source.owner_.lock().get() != this ||
        description.destination.owner_.lock().get() != this)
        return Failure{"post resources belong to another context"};
    const auto source_key = description.source.key_;
    const auto target_key = description.destination.key_;
    if (!image_native(source_key) || !image_native(target_key) ||
        source_key.slot == target_key.slot)
        return Failure{"post requires distinct live images"};
    auto &source = impl_->entries[source_key.slot];
    auto &target = impl_->entries[target_key.slot];
    if (!std::isfinite(description.exposure) || description.exposure < 0 ||
        description.exposure > 64)
        return Failure{"post exposure must be finite in 0..64"};
    if (source.aspect != VK_IMAGE_ASPECT_COLOR_BIT || source.layers != 1 ||
        !source.linear_filterable || (source.image_usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0 ||
        source.image_layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        return Failure{"post source must be an initialized filterable 2D color image"};
    if (target.layers != 1 || target.mip_levels != 1 ||
        (target.format != VK_FORMAT_R8G8B8A8_UNORM && target.format != VK_FORMAT_R8G8B8A8_SRGB) ||
        (target.image_usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0)
        return Failure{"post target must be a single-mip RGBA8 color attachment"};
    if (!impl_->post_pipeline)
        impl_->post_pipeline = std::make_unique<PostPipeline>(impl_->device);
    auto recording = impl_->begin(description.wait);
    if (const auto *failure = std::get_if<Failure>(&recording))
        return *failure;
    auto &frame = *std::get<Impl::Frame *>(recording);
    VkImageMemoryBarrier2 input{};
    input.image = source.image;
    input.oldLayout = source.image_layout;
    input.newLayout = source.image_layout;
    input.subresourceRange.aspectMask = source.aspect;
    input.subresourceRange.layerCount = source.layers;
    input.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    input.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    input.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    input.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    Impl::image_barrier(frame, input, source.mip_levels);
    VkImageMemoryBarrier2 output{};
    output.image = target.image;
    output.oldLayout = target.image_layout;
    output.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    output.subresourceRange.aspectMask = target.aspect;
    output.subresourceRange.layerCount = target.layers;
    output.srcStageMask = target.image_layout == VK_IMAGE_LAYOUT_UNDEFINED
                              ? VK_PIPELINE_STAGE_2_NONE
                              : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    output.srcAccessMask = target.image_layout == VK_IMAGE_LAYOUT_UNDEFINED
                               ? VK_ACCESS_2_NONE
                               : VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    output.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    output.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    Impl::image_barrier(frame, output, target.mip_levels);
    VkRenderingAttachmentInfo color{};
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageView = target.view;
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent = target.image_extent;
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color;
    vkCmdBeginRendering(frame.commands, &rendering);
    const shader::PostConstants constants{{description.exposure, description.fxaa ? 1.0F : 0.0F,
                                           1.0F / static_cast<float>(source.image_extent.width),
                                           1.0F / static_cast<float>(source.image_extent.height)}};
    impl_->post_pipeline->record({.commands = frame.commands,
                                  .frame = static_cast<std::size_t>(&frame - impl_->frames.data()),
                                  .format = target.format,
                                  .source = source.view,
                                  .extent = target.image_extent,
                                  .constants = constants});
    vkCmdEndRendering(frame.commands);
    output.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    output.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    output.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    output.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    output.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    output.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    Impl::image_barrier(frame, output, target.mip_levels);
    const auto next = impl_->submit(frame, description.wait);
    source.last_use = next;
    target.last_use = next;
    target.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    return Submission{weak_from_this(), next};
}
std::variant<Submission, Failure> Resources::mesh(MeshPass description) {
    if (description.draws.empty() || description.draws.size() > MeshPipeline::draw_limit)
        return Failure{"mesh draw count is outside 1..256"};
    if (description.vertices.owner_.lock().get() != this ||
        description.indices.owner_.lock().get() != this || !native(description.vertices.key_) ||
        !native(description.indices.key_))
        return Failure{"mesh geometry must be live same-context buffers"};
    const auto vertex_key = description.vertices.key_, index_key = description.indices.key_;
    auto &vertices = impl_->entries[vertex_key.slot];
    auto &indices = impl_->entries[index_key.slot];
    if ((vertices.usage & VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) == 0 ||
        (indices.usage & VK_BUFFER_USAGE_INDEX_BUFFER_BIT) == 0 ||
        vertices.size % sizeof(shader::Vertex) != 0 || indices.size % sizeof(std::uint32_t) != 0)
        return Failure{"mesh geometry has incompatible usage or record size"};
    const auto owned_image = [&](const Image &image) {
        return image.owner_.lock().get() == this && image_native(image.key_) != VK_NULL_HANDLE;
    };
    if (!owned_image(description.color) || !owned_image(description.depth) ||
        description.color.key_.slot == description.depth.key_.slot)
        return Failure{"mesh attachments must be distinct live same-context images"};
    const auto color_key = description.color.key_, depth_key = description.depth.key_;
    auto &color = impl_->entries[color_key.slot];
    auto &depth = impl_->entries[depth_key.slot];
    if (color.format != VK_FORMAT_R16G16B16A16_SFLOAT || color.mip_levels != 1 ||
        color.layers != 1 || (color.image_usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0 ||
        (color.image_usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0 ||
        depth.format != VK_FORMAT_D32_SFLOAT || depth.mip_levels != 1 || depth.layers != 1 ||
        (depth.image_usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0 ||
        depth.image_extent.width != color.image_extent.width ||
        depth.image_extent.height != color.image_extent.height)
        return Failure{"mesh requires matching single-mip HDR/depth attachments"};
    std::array<bool, Impl::capacity> used{};
    used[vertex_key.slot] = used[index_key.slot] = used[color_key.slot] = used[depth_key.slot] =
        true;
    const auto sampled = [&](const Image &image, std::uint32_t layers, VkImageAspectFlags aspect,
                             bool filterable) {
        if (!owned_image(image) || image.key_.slot == color_key.slot ||
            image.key_.slot == depth_key.slot)
            return false;
        const auto &entry = impl_->entries[image.key_.slot];
        if (entry.layers != layers || entry.aspect != aspect ||
            (filterable && !entry.linear_filterable) ||
            (entry.image_usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0 ||
            entry.image_layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            return false;
        used[image.key_.slot] = true;
        return true;
    };
    if (!sampled(description.shadow, 1, VK_IMAGE_ASPECT_DEPTH_BIT, false) ||
        !sampled(description.irradiance, 6, VK_IMAGE_ASPECT_COLOR_BIT, true) ||
        !sampled(description.prefiltered, 6, VK_IMAGE_ASPECT_COLOR_BIT, true) ||
        !sampled(description.brdf, 1, VK_IMAGE_ASPECT_COLOR_BIT, true))
        return Failure{
            "mesh environment/shadow inputs must be initialized compatible sampled images"};
    const auto finite = [](const auto &values) {
        return std::all_of(values.begin(), values.end(),
                           [](float value) { return std::isfinite(value); });
    };
    if (description.lights.count > 16)
        return Failure{"mesh local light count exceeds 16"};
    for (const auto &light : description.lights.lights)
        if (!finite(light.positionRange) || !finite(light.colorIntensity))
            return Failure{"mesh light values must be finite"};
    const auto address = [](VkSamplerAddressMode mode) {
        return mode == VK_SAMPLER_ADDRESS_MODE_REPEAT ||
               mode == VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT ||
               mode == VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    };
    std::array<MeshNativeDraw, MeshPipeline::draw_limit> bindings{};
    for (std::size_t i = 0; i < description.draws.size(); ++i) {
        const auto &draw = description.draws[i];
        const auto &frame = draw.frame;
        const auto &material = draw.material;
        if (draw.index_count == 0 || draw.index_count % 3 != 0 ||
            static_cast<std::uint64_t>(draw.first_index) + draw.index_count >
                indices.size / sizeof(std::uint32_t) ||
            draw.vertex_offset < 0 ||
            static_cast<std::uint64_t>(draw.vertex_offset) >=
                vertices.size / sizeof(shader::Vertex))
            return Failure{"mesh indexed draw range is invalid"};
        if (static_cast<unsigned>(draw.facing) > 2 || material.flags[0] > 2 ||
            material.flags[1] > 31 || material.flags[2] > 1 || material.flags[3] != 0 ||
            !finite(material.baseColor) || !finite(material.emissiveAlphaCutoff) ||
            !finite(material.metallicRoughnessNormalOcclusion) || !finite(frame.model.columns) ||
            !finite(frame.viewProjection.columns) || !finite(frame.normalMatrix.columns) ||
            !finite(frame.lightViewProjection.columns) || !finite(frame.cameraPosition) ||
            !finite(frame.keyDirectionIntensity) || !finite(frame.keyColor))
            return Failure{"mesh constants are invalid or nonfinite"};
        for (std::size_t map = 0; map < 5; ++map) {
            if (!draw.maps[map] || !sampled(*draw.maps[map], 1, VK_IMAGE_ASPECT_COLOR_BIT, true))
                return Failure{"mesh material texture must be initialized same-context 2D color"};
            const auto &sampler = draw.samplers[map];
            if ((sampler.min != VK_FILTER_NEAREST && sampler.min != VK_FILTER_LINEAR) ||
                (sampler.mag != VK_FILTER_NEAREST && sampler.mag != VK_FILTER_LINEAR) ||
                (sampler.mip != VK_SAMPLER_MIPMAP_MODE_NEAREST &&
                 sampler.mip != VK_SAMPLER_MIPMAP_MODE_LINEAR) ||
                !address(sampler.u) || !address(sampler.v))
                return Failure{"mesh sampler is outside the fixed profile"};
            bindings[i].maps[map] = impl_->entries[draw.maps[map]->key_.slot].view;
        }
    }
    if (!impl_->mesh_pipeline)
        impl_->mesh_pipeline = std::make_unique<MeshPipeline>(impl_->device, impl_->physical);
    for (auto &uniform : impl_->mesh_uniforms)
        if (!uniform.native()) {
            auto created = create({impl_->mesh_pipeline->uniform_bytes(),
                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, MemoryAccess::upload});
            if (const auto *failure = std::get_if<Failure>(&created))
                return *failure;
            uniform = std::get<Buffer>(std::move(created));
        }
    auto recording = impl_->begin(description.wait);
    if (const auto *failure = std::get_if<Failure>(&recording))
        return *failure;
    auto &frame = *std::get<Impl::Frame *>(recording);
    const auto slot = static_cast<std::size_t>(&frame - impl_->frames.data());
    auto &uniform = impl_->mesh_uniforms[slot];
    const auto written = uniform.write(0, impl_->mesh_pipeline->prepare(description));
    if (const auto *failure = std::get_if<Failure>(&written))
        return *failure;
    used[uniform.key_.slot] = true;
    VkMemoryBarrier2 host{};
    host.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    host.srcStageMask = VK_PIPELINE_STAGE_2_HOST_BIT;
    host.srcAccessMask = VK_ACCESS_2_HOST_WRITE_BIT;
    host.dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
    host.dstAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT | VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT |
                         VK_ACCESS_2_INDEX_READ_BIT;
    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.memoryBarrierCount = 1;
    dependency.pMemoryBarriers = &host;
    vkCmdPipelineBarrier2(frame.commands, &dependency);
    for (std::size_t i = 0; i < used.size(); ++i) {
        const auto &entry = impl_->entries[i];
        if (!used[i] || !entry.image || i == color_key.slot || i == depth_key.slot)
            continue;
        VkImageMemoryBarrier2 barrier{};
        barrier.image = entry.image;
        barrier.oldLayout = barrier.newLayout = entry.image_layout;
        barrier.subresourceRange.aspectMask = entry.aspect;
        barrier.subresourceRange.layerCount = entry.layers;
        barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        barrier.srcAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
        barrier.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
        Impl::image_barrier(frame, barrier, entry.mip_levels);
    }
    const auto attachment = [&](const Impl::Entry &entry, VkImageLayout layout) {
        VkImageMemoryBarrier2 barrier{};
        barrier.image = entry.image;
        barrier.oldLayout = entry.image_layout;
        barrier.newLayout = layout;
        barrier.subresourceRange.aspectMask = entry.aspect;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcStageMask = entry.image_layout == VK_IMAGE_LAYOUT_UNDEFINED
                                   ? VK_PIPELINE_STAGE_2_NONE
                                   : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
        barrier.srcAccessMask = entry.image_layout == VK_IMAGE_LAYOUT_UNDEFINED
                                    ? VK_ACCESS_2_NONE
                                    : VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
        const bool depth_attachment = entry.aspect == VK_IMAGE_ASPECT_DEPTH_BIT;
        barrier.dstStageMask = depth_attachment ? VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT |
                                                      VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT
                                                : VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
        barrier.dstAccessMask = depth_attachment
                                    ? VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                                          VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT
                                    : VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
        Impl::image_barrier(frame, barrier, 1);
    };
    attachment(color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    attachment(depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo color_info{};
    color_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color_info.imageView = color.view;
    color_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_info.clearValue.color.float32[3] = 1;
    VkRenderingAttachmentInfo depth_info{};
    depth_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depth_info.imageView = depth.view;
    depth_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    depth_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depth_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depth_info.clearValue.depthStencil.depth = 1;
    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent = color.image_extent;
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color_info;
    rendering.pDepthAttachment = &depth_info;
    vkCmdBeginRendering(frame.commands, &rendering);
    impl_->mesh_pipeline->record({frame.commands, slot, uniform.native(), vertices.buffer,
                                  indices.buffer, color.image_extent, description.shadow.view(),
                                  description.irradiance.view(), description.prefiltered.view(),
                                  description.brdf.view(), description.draws,
                                  std::span(bindings).first(description.draws.size())});
    vkCmdEndRendering(frame.commands);
    VkImageMemoryBarrier2 output{};
    output.image = color.image;
    output.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    output.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    output.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    output.subresourceRange.layerCount = 1;
    output.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    output.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    output.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    output.dstAccessMask = VK_ACCESS_2_SHADER_SAMPLED_READ_BIT;
    Impl::image_barrier(frame, output, 1);
    const auto next = impl_->submit(frame, description.wait);
    for (std::size_t i = 0; i < used.size(); ++i)
        if (used[i])
            impl_->entries[i].last_use = next;
    color.image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    depth.image_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    return Submission{weak_from_this(), next};
}
std::variant<std::monostate, Failure> Resources::presentation_ready() {
    if (impl_->collect() < impl_->external_wait_submission)
        return Failure{
            "presentation cannot queue behind unresolved external timeline dependencies"};
    return std::monostate{};
}
std::variant<GraphicsUse, Failure> Resources::prepare_frame(ImageFrame description) {
    if (description.source.owner_.lock().get() != this)
        return Failure{"frame image belongs to another context"};
    const auto key = description.source.key_;
    if (!image_native(key))
        return Failure{"frame image is stale or closed"};
    if (!std::isfinite(description.exposure) || description.exposure < 0 ||
        description.exposure > 64)
        return Failure{"frame exposure must be finite in 0..64"};
    if (impl_->submitted == UINT64_MAX)
        return Failure{"resource submission counter exhausted"};
    const auto &entry = impl_->entries[key.slot];
    if (entry.aspect != VK_IMAGE_ASPECT_COLOR_BIT || entry.layers != 1 ||
        !entry.linear_filterable || (entry.image_usage & VK_IMAGE_USAGE_SAMPLED_BIT) == 0 ||
        entry.image_layout != VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
        return Failure{"frame image must be initialized filterable 2D color"};
    if (!impl_->post_pipeline)
        impl_->post_pipeline = std::make_unique<PostPipeline>(impl_->device);
    return GraphicsUse{key,
                       entry.image,
                       entry.view,
                       entry.image_extent,
                       entry.mip_levels,
                       impl_->timeline,
                       impl_->submitted + 1};
}
void Resources::record_frame(const PostDraw &draw) const { impl_->post_pipeline->record(draw); }
Submission Resources::submitted_frame(const GraphicsUse &use) noexcept {
    impl_->entries[use.key.slot].last_use = use.serial;
    impl_->submitted = use.serial;
    return Submission{weak_from_this(), use.serial};
}
std::variant<bool, Failure> Resources::completed(const Submission &submission) {
    if (submission.owner_.lock().get() != this || submission.value_ == 0 ||
        submission.value_ > impl_->submitted)
        return Failure{"invalid resource submission"};
    return impl_->collect() >= submission.value_;
}
std::variant<std::monostate, Failure> Resources::write(detail::ResourceKey key, VkDeviceSize offset,
                                                       std::span<const std::byte> bytes) {
    if (!native(key))
        return Failure{"buffer is closed or stale"};
    const auto &entry = impl_->entries[key.slot];
    if (entry.memory != MemoryAccess::upload)
        return Failure{"buffer is not CPU-writable"};
    if (offset > entry.size || bytes.size() > entry.size - offset)
        return Failure{"buffer write range exceeds size"};
    if (entry.last_use > impl_->collect())
        return Failure{"buffer is still in GPU use"};
    if (!bytes.empty()) {
        VmaAllocationInfo info{};
        vmaGetAllocationInfo(impl_->allocator, entry.allocation, &info);
        std::memcpy(static_cast<std::byte *>(info.pMappedData) + static_cast<std::size_t>(offset),
                    bytes.data(), bytes.size());
        resource_check(vmaFlushAllocation(impl_->allocator, entry.allocation, offset, bytes.size()),
                       "flush upload memory");
    }
    return std::monostate{};
}
std::variant<std::monostate, Failure> Resources::read(detail::ResourceKey key, VkDeviceSize offset,
                                                      std::span<std::byte> bytes) {
    if (!native(key))
        return Failure{"buffer is closed or stale"};
    const auto &entry = impl_->entries[key.slot];
    if (entry.memory != MemoryAccess::readback)
        return Failure{"buffer is not CPU-readable"};
    if (offset > entry.size || bytes.size() > entry.size - offset)
        return Failure{"buffer read range exceeds size"};
    if (entry.last_use > impl_->collect())
        return Failure{"buffer is still in GPU use"};
    if (!bytes.empty()) {
        resource_check(
            vmaInvalidateAllocation(impl_->allocator, entry.allocation, offset, bytes.size()),
            "invalidate readback memory");
        VmaAllocationInfo info{};
        vmaGetAllocationInfo(impl_->allocator, entry.allocation, &info);
        std::memcpy(bytes.data(),
                    static_cast<const std::byte *>(info.pMappedData) +
                        static_cast<std::size_t>(offset),
                    bytes.size());
    }
    return std::monostate{};
}
ResourceStatistics Resources::statistics() {
    (void)impl_->collect();
    ResourceStatistics result{};
    for (const auto &entry : impl_->entries) {
        if (entry.buffer || entry.image) {
            if (entry.retired)
                ++result.retiring;
            else
                ++result.live;
        }
    }
    VmaTotalStatistics totals{};
    vmaCalculateStatistics(impl_->allocator, &totals);
    result.allocation_bytes = totals.total.statistics.allocationBytes;
    result.reserved_bytes = totals.total.statistics.blockBytes;
    return result;
}
void Resources::release(detail::ResourceKey key) noexcept {
    if (impl_->find(key))
        impl_->entries[key.slot].retired = true;
}
void Resources::close() noexcept {
    if (!impl_->allocator)
        return;
    for (auto &frame : impl_->frames) {
        if (frame.pool)
            vkDestroyCommandPool(impl_->device, frame.pool, nullptr);
        frame = {};
    }
    if (impl_->post_pipeline) {
        impl_->post_pipeline->close();
        impl_->post_pipeline.reset();
    }
    if (impl_->mesh_pipeline) {
        impl_->mesh_pipeline->close();
        impl_->mesh_pipeline.reset();
    }
    impl_->mesh_uniforms = {};
    for (auto &entry : impl_->entries)
        impl_->destroy(entry);
    if (impl_->timeline)
        vkDestroySemaphore(impl_->device, impl_->timeline, nullptr);
    impl_->timeline = VK_NULL_HANDLE;
    vmaDestroyAllocator(impl_->allocator);
    impl_->allocator = VK_NULL_HANDLE;
}
Buffer::Buffer(std::weak_ptr<Resources> owner, detail::ResourceKey key)
    : owner_(std::move(owner)), key_(key) {}
Buffer::~Buffer() { release(); }
Buffer::Buffer(Buffer &&other) noexcept
    : owner_(std::move(other.owner_)), key_(std::exchange(other.key_, {})) {}
Buffer &Buffer::operator=(Buffer &&other) noexcept {
    if (this != &other) {
        release();
        owner_ = std::move(other.owner_);
        key_ = std::exchange(other.key_, {});
    }
    return *this;
}
void Buffer::release() noexcept {
    if (const auto owner = owner_.lock())
        owner->release(key_);
    owner_.reset();
    key_ = {};
}
std::variant<std::monostate, Failure> Buffer::write(VkDeviceSize offset,
                                                    std::span<const std::byte> bytes) {
    try {
        if (const auto owner = owner_.lock())
            return owner->write(key_, offset, bytes);
        return Failure{"buffer owner is closed"};
    } catch (const std::exception &error) {
        return Failure{error.what()};
    }
}
std::variant<std::monostate, Failure> Buffer::read(VkDeviceSize offset,
                                                   std::span<std::byte> bytes) const {
    try {
        if (const auto owner = owner_.lock())
            return owner->read(key_, offset, bytes);
        return Failure{"buffer owner is closed"};
    } catch (const std::exception &error) {
        return Failure{error.what()};
    }
}
VkBuffer Buffer::native() const {
    const auto owner = owner_.lock();
    return owner ? owner->native(key_) : VK_NULL_HANDLE;
}
Image::Image(std::weak_ptr<Resources> owner, detail::ResourceKey key)
    : owner_(std::move(owner)), key_(key) {}
Image::~Image() { release(); }
Image::Image(Image &&other) noexcept
    : owner_(std::move(other.owner_)), key_(std::exchange(other.key_, {})) {}
Image &Image::operator=(Image &&other) noexcept {
    if (this != &other) {
        release();
        owner_ = std::move(other.owner_);
        key_ = std::exchange(other.key_, {});
    }
    return *this;
}
void Image::release() noexcept {
    if (const auto owner = owner_.lock())
        owner->release(key_);
    owner_.reset();
    key_ = {};
}
VkImage Image::native() const {
    const auto owner = owner_.lock();
    return owner ? owner->image_native(key_) : VK_NULL_HANDLE;
}
VkImageView Image::view() const {
    const auto owner = owner_.lock();
    return owner ? owner->image_view(key_) : VK_NULL_HANDLE;
}
} // namespace ngen::render
