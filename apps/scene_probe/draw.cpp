#include "draw.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>

namespace ngen::probe {
namespace {
using namespace render;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class T> T take(std::variant<T, Failure> value) {
    if (const auto *failure = std::get_if<Failure>(&value))
        throw std::runtime_error(failure->message);
    return std::get<T>(std::move(value));
}
void wait(Context &context, const Submission &submission) {
    const auto start = SDL_GetTicks();
    while (!take(context.submission_complete(submission))) {
        require(SDL_GetTicks() - start < 5000, "scene draw completion timed out");
        SDL_Delay(1);
    }
}
class DrawGate {
  public:
    explicit DrawGate(Context &context) : context_(context) {
        VkSemaphoreTypeCreateInfo type{};
        type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = &type;
        require(vkCreateSemaphore(context.device(), &info, nullptr, &handle) == VK_SUCCESS,
                "frame gate creation failed");
    }
    ~DrawGate() {
        if (!signal() ||
            (submitted && !completed && vkQueueWaitIdle(context_.queue()) != VK_SUCCESS))
            return;
        vkDestroySemaphore(context_.device(), handle, nullptr);
    }
    bool signal() {
        if (signaled_)
            return true;
        VkSemaphoreSignalInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        info.semaphore = handle;
        info.value = 1;
        signaled_ = vkSignalSemaphore(context_.device(), &info) == VK_SUCCESS;
        return signaled_;
    }
    VkSemaphore handle{};
    bool submitted{}, completed{};

  private:
    Context &context_;
    bool signaled_{};
};
} // namespace
bool draw_scene(render::Context &context, render::ScenePin &version,
                const render::ScenePin &active) {
    using namespace render;
    const auto package = version->package();
    constexpr VkExtent2D extent{256, 128};
    auto color = take(
        context.create_image({extent, VK_FORMAT_R16G16B16A16_SFLOAT,
                              static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT}));
    auto depth = take(context.create_image(
        {extent, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT}));
    auto white =
        take(context.create_image({{1, 1},
                                   VK_FORMAT_R8G8B8A8_UNORM,
                                   static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT}));
    auto cube =
        take(context.create_image({{1, 1},
                                   VK_FORMAT_R8G8B8A8_UNORM,
                                   static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                                   1,
                                   true}));
    auto shadow =
        take(context.create_image({{1, 1},
                                   VK_FORMAT_D32_SFLOAT,
                                   static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                                       VK_IMAGE_USAGE_TRANSFER_DST_BIT}));
    const auto upload = [&](const Image &target, std::span<const std::byte> bytes) {
        auto staging = take(context.create_buffer(
            {bytes.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAccess::upload}));
        (void)take(staging.write(0, bytes));
        wait(context, take(context.copy_to_image({staging, target})));
    };
    const std::array<std::byte, 4> pixel{std::byte{255}, std::byte{255}, std::byte{255},
                                         std::byte{255}};
    const std::array<std::byte, 24> black{};
    const float depth_one = 1;
    upload(white, pixel);
    upload(cube, black);
    upload(shadow, std::as_bytes(std::span{&depth_one, 1}));
    SceneTargets targets{color, depth, shadow, cube, cube, white, white};
    SceneView view{};
    const auto &bounds = package->bounds;
    std::array<float, 3> center{};
    float radius = .5F;
    for (std::size_t i = 0; i < 3; ++i) {
        center[i] = (bounds.minimum[i] + bounds.maximum[i]) * .5F;
        radius = std::max(radius, (bounds.maximum[i] - bounds.minimum[i]) * .5F);
    }
    const std::array<float, 3> right{.70710678F, 0, -.70710678F},
        up{-.40824829F, .81649658F, -.40824829F}, back{.57735027F, .57735027F, .57735027F};
    float r_center = 0, u_center = 0, b_center = 0;
    for (std::size_t i = 0; i < 3; ++i) {
        view.view_projection.columns[i * 4] = right[i] / (radius * 1.8F);
        view.view_projection.columns[i * 4 + 1] = -up[i] / (radius * .9F);
        view.view_projection.columns[i * 4 + 2] = -back[i] / (radius * 6);
        r_center += right[i] * center[i];
        u_center += up[i] * center[i];
        b_center += back[i] * center[i];
        view.camera_position[i] = center[i] + back[i] * radius * 3;
    }
    view.view_projection.columns[12] = -r_center / (radius * 1.8F);
    view.view_projection.columns[13] = u_center / (radius * .9F);
    view.view_projection.columns[14] = .5F + b_center / (radius * 6);
    view.view_projection.columns[15] = 1;
    view.light_view_projection = view.view_projection;
    view.key_direction_intensity = {-1, -1, -1, 1};
    SceneRenderer renderer;
    auto invalid = view;
    invalid.view_projection.columns[0] = std::numeric_limits<float>::quiet_NaN();
    require(std::holds_alternative<Failure>(renderer.draw(context, version, invalid, targets)) &&
                take(renderer.collect(context)) == 0,
            "invalid scene view changed frame ownership");
    DrawGate gate(context);
    const std::weak_ptr<const SceneVersion> weak = version;
    auto first = renderer.draw(context, version, view, targets, {gate.handle, 1});
    if (std::holds_alternative<Failure>(first))
        require(take(renderer.collect(context)) == 0,
                "failed scene draw published an owning frame pin");
    const auto submission = take(std::move(first));
    gate.submitted = true;
    const auto second = take(renderer.draw(context, active, view, targets));
    const auto busy = renderer.draw(context, active, view, targets);
    const auto *backpressure = std::get_if<Failure>(&busy);
    require(backpressure && backpressure->message == "scene frame pins are busy",
            "scene frame pin bound was not enforced");
    version.reset();
    require(!weak.expired() && take(renderer.collect(context)) == 2,
            "frame version was not retained while pending");
    require(gate.signal(), "scene frame gate did not signal");
    wait(context, submission);
    wait(context, second);
    gate.completed = true;
    require(take(renderer.collect(context)) == 0 && weak.expired(),
            "completed frame version did not release its owning pin");
    constexpr auto bytes = VkDeviceSize{extent.width} * extent.height * 8;
    auto readback = take(
        context.create_buffer({bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAccess::readback}));
    wait(context, take(context.copy_from_image({color, readback})));
    std::array<std::uint16_t, std::size_t{extent.width} * extent.height * 4> actual{};
    (void)take(readback.read(0, std::as_writable_bytes(std::span(actual))));
    std::size_t lit = 0;
    for (std::size_t i = 0; i < actual.size(); i += 4) {
        bool nonzero = false;
        for (std::size_t channel = 0; channel < 3; ++channel) {
            require((actual[i + channel] & 0x7c00U) != 0x7c00U, "scene HDR output is nonfinite");
            nonzero = nonzero || actual[i + channel] != 0;
        }
        if (nonzero)
            ++lit;
    }
    require(lit > 0, "scene geometry did not draw any material pixels");
    require(SDL_ShowWindow(context.window()), "scene presentation window did not show");
    const auto start = SDL_GetTicks();
    bool presented = false;
    while (SDL_GetTicks() - start < 2000) {
        SDL_Event event{};
        while (SDL_PollEvent(&event)) {
        }
        const auto frame = take(context.render_frame(ImageFrame{color}));
        if (frame.outcome == FrameOutcome::queued) {
            wait(context, frame.resources);
            presented = true;
            break;
        }
        SDL_Delay(1);
    }
    require(presented, "scene HDR output did not reach a queued WSI frame");
    std::printf("checked_scene_pbr=1 frame_version_pin=1 frame_versions=2 frame_pin_retired=1 "
                "scene_lit_pixels=%zu "
                "scene_wsi_queued=1\n",
                lit);
    return true;
}
} // namespace ngen::probe
