#include "render/presentation.hpp"
#include <cstdio>
#include <stdexcept>

namespace ngen::render {
namespace {
class VulkanFailure : public std::runtime_error {
  public:
    VulkanFailure(VkResult result, const char *operation)
        : std::runtime_error(std::string(operation) + ": VkResult " + std::to_string(result)),
          result_(result) {}
    [[nodiscard]] VkResult result() const { return result_; }

  private:
    VkResult result_;
};
void checked(VkResult result, const char *operation) {
    if (result != VK_SUCCESS)
        throw VulkanFailure(result, operation);
}
bool fence_ready(VkDevice device, VkFence fence, bool wait) {
    const auto result = wait ? vkWaitForFences(device, 1, &fence, VK_TRUE, UINT64_MAX)
                             : vkGetFenceStatus(device, fence);
    if (!wait && result == VK_NOT_READY)
        return false;
    checked(result, "wait for frame/presentation retirement");
    return true;
}
} // namespace
void Presentation::initialize() {
    const auto device = owner_.device();
    release_images_ = reinterpret_cast<PFN_vkReleaseSwapchainImagesEXT>(
        vkGetDeviceProcAddr(device, "vkReleaseSwapchainImagesEXT"));
    if (!release_images_)
        throw std::runtime_error("swapchain maintenance release entry point unavailable");
    for (auto &frame : frames_) {
        VkCommandPoolCreateInfo pool{};
        pool.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pool.queueFamilyIndex = owner_.queue_family();
        checked(vkCreateCommandPool(device, &pool, nullptr, &frame.pool), "create frame pool");
        VkCommandBufferAllocateInfo commands{};
        commands.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commands.commandPool = frame.pool;
        commands.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commands.commandBufferCount = 1;
        checked(vkAllocateCommandBuffers(device, &commands, &frame.commands),
                "allocate frame commands");
        VkFenceCreateInfo fence{};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        checked(vkCreateFence(device, &fence, nullptr, &frame.acquired_fence),
                "create acquisition fence");
        checked(vkCreateFence(device, &fence, nullptr, &frame.submitted_fence),
                "create submission fence");
    }
}
void Presentation::release_acquired() {
    std::array<std::uint32_t, image_limit> indices{};
    std::uint32_t count = 0;
    for (std::uint32_t i = 0; i < image_count_; ++i)
        if (images_[i].acquired)
            indices[count++] = i;
    if (count != 0) {
        VkReleaseSwapchainImagesInfoEXT release{};
        release.sType = VK_STRUCTURE_TYPE_RELEASE_SWAPCHAIN_IMAGES_INFO_EXT;
        release.swapchain = swapchain_;
        release.imageIndexCount = count;
        release.pImageIndices = indices.data();
        checked(release_images_(owner_.device(), &release), "release acquired images");
    }
    for (auto &image : images_)
        image.acquired = false;
    for (auto &frame : frames_)
        frame.acquired.reset();
}
bool Presentation::retire(bool wait) {
    for (auto &frame : frames_) {
        if (frame.acquired && !fence_ready(owner_.device(), frame.acquired_fence, wait))
            return false;
        if (frame.submitted && !fence_ready(owner_.device(), frame.submitted_fence, wait))
            return false;
    }
    for (auto &image : images_) {
        if (image.present_pending && !fence_ready(owner_.device(), image.retired, wait))
            return false;
    }
    release_acquired();
    for (auto &frame : frames_)
        frame.submitted = false;
    for (auto &image : images_)
        image.present_pending = false;
    return true;
}
void Presentation::destroy_swapchain() noexcept {
    const auto device = owner_.device();
    for (auto &image : images_) {
        if (image.view)
            vkDestroyImageView(device, image.view, nullptr);
        if (image.ready)
            vkDestroySemaphore(device, image.ready, nullptr);
        if (image.retired)
            vkDestroyFence(device, image.retired, nullptr);
        image = {};
    }
    if (swapchain_)
        vkDestroySwapchainKHR(device, swapchain_, nullptr);
    swapchain_ = VK_NULL_HANDLE;
    image_count_ = 0;
}
void Presentation::rebuild(const SurfacePlan &plan) {
    if (plan.image_count > image_limit)
        throw std::runtime_error("surface exceeds the 16-image presentation budget");
    if (generation_ == UINT64_MAX)
        throw std::runtime_error("swapchain generation exhausted");
    destroy_swapchain(); // All submission/acquisition/presentation fences have retired.
    const VkSwapchainCreateInfoKHR info{.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                        .pNext = nullptr,
                                        .flags = 0,
                                        .surface = owner_.surface(),
                                        .minImageCount = plan.image_count,
                                        .imageFormat = plan.format.format,
                                        .imageColorSpace = plan.format.colorSpace,
                                        .imageExtent = plan.extent,
                                        .imageArrayLayers = 1,
                                        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                        .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
                                        .queueFamilyIndexCount = 0,
                                        .pQueueFamilyIndices = nullptr,
                                        .preTransform = plan.transform,
                                        .compositeAlpha = plan.alpha,
                                        .presentMode = plan.present_mode,
                                        .clipped = VK_TRUE,
                                        .oldSwapchain = VK_NULL_HANDLE};
    const auto device = owner_.device();
    checked(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain_), "create swapchain");
    std::uint32_t count = 0;
    checked(vkGetSwapchainImagesKHR(device, swapchain_, &count, nullptr), "count swapchain images");
    if (count == 0 || count > image_limit)
        throw std::runtime_error("swapchain image count outside 1..16");
    std::array<VkImage, image_limit> handles{};
    checked(vkGetSwapchainImagesKHR(device, swapchain_, &count, handles.data()),
            "get swapchain images");
    if (count == 0 || count > image_limit)
        throw std::runtime_error("swapchain image count changed outside 1..16");
    image_count_ = count;
    for (std::uint32_t i = 0; i < image_count_; ++i) {
        auto &image = images_[i];
        image.image = handles[i];
        VkImageViewCreateInfo view{};
        view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        view.image = image.image;
        view.viewType = VK_IMAGE_VIEW_TYPE_2D;
        view.format = plan.format.format;
        view.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        checked(vkCreateImageView(device, &view, nullptr, &image.view), "create swapchain view");
        VkSemaphoreCreateInfo semaphore{};
        semaphore.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        checked(vkCreateSemaphore(device, &semaphore, nullptr, &image.ready),
                "create present semaphore");
        VkFenceCreateInfo fence{};
        fence.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        checked(vkCreateFence(device, &fence, nullptr, &image.retired),
                "create present retirement fence");
    }
    extent_ = plan.extent;
    ++generation_;
    rebuild_requested_ = false;
}
void Presentation::record(Frame &frame, const Image &image) {
    checked(vkResetCommandPool(owner_.device(), frame.pool, 0), "reset frame pool");
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    checked(vkBeginCommandBuffer(frame.commands, &begin), "begin frame");
    VkImageMemoryBarrier2 barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image.image;
    barrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkDependencyInfo dependency{};
    dependency.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency.imageMemoryBarrierCount = 1;
    dependency.pImageMemoryBarriers = &barrier;
    vkCmdPipelineBarrier2(frame.commands, &dependency);
    VkRenderingAttachmentInfo color{};
    color.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    color.imageView = image.view;
    color.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color.clearValue.color = {{0.04F, 0.09F, 0.18F, 1.0F}};
    VkRenderingInfo rendering{};
    rendering.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
    rendering.renderArea.extent = extent_;
    rendering.layerCount = 1;
    rendering.colorAttachmentCount = 1;
    rendering.pColorAttachments = &color;
    vkCmdBeginRendering(frame.commands, &rendering);
    vkCmdEndRendering(frame.commands);
    barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
    barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstStageMask = VK_PIPELINE_STAGE_2_NONE;
    barrier.dstAccessMask = VK_ACCESS_2_NONE;
    barrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier2(frame.commands, &dependency);
    checked(vkEndCommandBuffer(frame.commands), "end frame");
}
FrameOutcome Presentation::draw() {
    int width = 0, height = 0;
    if (!SDL_GetWindowSizeInPixels(owner_.window(), &width, &height))
        throw std::runtime_error(SDL_GetError());
    if (width <= 0 || height <= 0 || (SDL_GetWindowFlags(owner_.window()) & SDL_WINDOW_MINIMIZED))
        return FrameOutcome::skipped;
    const VkExtent2D drawable{static_cast<std::uint32_t>(width),
                              static_cast<std::uint32_t>(height)};
    if (rebuild_requested_ || drawable.width != drawable_.width ||
        drawable.height != drawable_.height) {
        const auto selection = owner_.inspect_surface();
        if (const auto *failure = std::get_if<SurfaceFailure>(&selection)) {
            if (failure->issue == SurfaceIssue::not_drawable)
                return FrameOutcome::skipped;
            throw std::runtime_error(failure->message);
        }
        if (!retire(false))
            return FrameOutcome::skipped;
        rebuild(std::get<SurfacePlan>(selection));
        drawable_ = drawable;
    }
    auto &frame = frames_[frame_index_];
    const auto device = owner_.device();
    if (frame.submitted) {
        if (!fence_ready(device, frame.submitted_fence, false))
            return FrameOutcome::skipped;
        frame.submitted = false;
    }
    if (!frame.acquired) {
        checked(vkResetFences(device, 1, &frame.acquired_fence), "reset acquisition fence");
        std::uint32_t index = 0;
        const auto result = vkAcquireNextImageKHR(device, swapchain_, 0, VK_NULL_HANDLE,
                                                  frame.acquired_fence, &index);
        if (result == VK_NOT_READY || result == VK_TIMEOUT)
            return FrameOutcome::skipped;
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            rebuild_requested_ = true;
            return FrameOutcome::skipped;
        }
        if (result != VK_SUBOPTIMAL_KHR)
            checked(result, "acquire swapchain image");
        if (index >= image_count_)
            throw std::runtime_error("acquired image index outside the swapchain");
        frame.acquired = index;
        images_[index].acquired = true;
        frame.suboptimal = result == VK_SUBOPTIMAL_KHR;
    }
    if (!fence_ready(device, frame.acquired_fence, false))
        return FrameOutcome::skipped;
    auto &image = images_[*frame.acquired];
    if (image.present_pending) {
        if (!fence_ready(device, image.retired, false))
            return FrameOutcome::skipped;
        image.present_pending = false;
    }
    record(frame, image);
    checked(vkResetFences(device, 1, &frame.submitted_fence), "reset submission fence");
    VkCommandBufferSubmitInfo command{};
    command.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command.commandBuffer = frame.commands;
    command.deviceMask = 1;
    VkSemaphoreSubmitInfo signal{};
    signal.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signal.semaphore = image.ready;
    signal.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    VkSubmitInfo2 submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &command;
    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signal;
    checked(vkQueueSubmit2(owner_.queue(), 1, &submit, frame.submitted_fence), "submit frame");
    frame.submitted = true;
    checked(vkResetFences(device, 1, &image.retired), "reset present retirement fence");
    VkSwapchainPresentFenceInfoEXT fence{};
    fence.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_PRESENT_FENCE_INFO_EXT;
    fence.swapchainCount = 1;
    fence.pFences = &image.retired;
    const auto index = *frame.acquired;
    VkPresentInfoKHR present{};
    present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present.pNext = &fence;
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &image.ready;
    present.swapchainCount = 1;
    present.pSwapchains = &swapchain_;
    present.pImageIndices = &index;
    const auto result = vkQueuePresentKHR(owner_.queue(), &present);
    if (result != VK_ERROR_OUT_OF_HOST_MEMORY && result != VK_ERROR_OUT_OF_DEVICE_MEMORY &&
        result != VK_ERROR_DEVICE_LOST) {
        image.acquired = false;
        image.present_pending = true;
        frame.acquired.reset();
    }
    if (result != VK_SUBOPTIMAL_KHR && result != VK_ERROR_OUT_OF_DATE_KHR)
        checked(result, "queue presentation");
    if (frame.suboptimal || result == VK_SUBOPTIMAL_KHR || result == VK_ERROR_OUT_OF_DATE_KHR)
        rebuild_requested_ = true;
    frame.suboptimal = false;
    frame_index_ = (frame_index_ + 1) % frames_.size();
    return result == VK_ERROR_OUT_OF_DATE_KHR ? FrameOutcome::skipped : FrameOutcome::queued;
}
VkResult Presentation::close(VkResult device_idle) noexcept {
    if (!owner_.device())
        return VK_SUCCESS;
    if (device_idle != VK_ERROR_DEVICE_LOST) {
        try {
            (void)retire(true);
        } catch (const VulkanFailure &error) {
            std::fprintf(stderr, "Vulkan presentation shutdown: %s\n", error.what());
            if (error.result() != VK_ERROR_DEVICE_LOST)
                return error.result();
        } catch (const std::exception &error) {
            std::fprintf(stderr, "Vulkan presentation shutdown: %s\n", error.what());
            return VK_ERROR_UNKNOWN;
        }
    }
    destroy_swapchain();
    for (auto &frame : frames_) {
        if (frame.pool)
            vkDestroyCommandPool(owner_.device(), frame.pool, nullptr);
        if (frame.acquired_fence)
            vkDestroyFence(owner_.device(), frame.acquired_fence, nullptr);
        if (frame.submitted_fence)
            vkDestroyFence(owner_.device(), frame.submitted_fence, nullptr);
        frame = {};
    }
    return VK_SUCCESS;
}
Presentation::~Presentation() { (void)close(VK_SUCCESS); }
} // namespace ngen::render
