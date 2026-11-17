#include "draw.hpp"
#include "probe/capture.hpp"
#include "render/environment.hpp"
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
bool draw_scene(render::Context &context, render::ScenePin &version, const render::ScenePin &active,
                const std::filesystem::path &capture) {
    using namespace render;
    const auto package = version->package();
    const VkExtent2D extent = capture.empty() ? VkExtent2D{256, 128} : VkExtent2D{1280, 720};
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
    auto shadow =
        take(context.create_image({{1024, 1024},
                                   VK_FORMAT_D32_SFLOAT,
                                   static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                                       VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT}));
    const auto upload = [&](const Image &target, std::span<const std::byte> bytes) {
        auto staging = take(context.create_buffer(
            {bytes.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAccess::upload}));
        (void)take(staging.write(0, bytes));
        wait(context, take(context.copy_to_image({staging, target})));
    };
    const std::array<std::byte, 4> pixel{std::byte{255}, std::byte{255}, std::byte{255},
                                         std::byte{255}};
    upload(white, pixel);
    const auto generated = take(generate_environment(Sky{}));
    std::array<Image, 3> environment;
    for (std::size_t i = 0; i < environment.size(); ++i) {
        environment[i] = take(context.create_image(generated.images[i].description));
        upload(environment[i], generated.images[i].texels);
    }
    SceneTargets targets{color,          depth,          shadow, environment[0],
                         environment[1], environment[2], white};
    SceneView view{};
    const auto &bounds = package->bounds;
    std::array<float, 3> center{};
    float radius = .5F;
    for (std::size_t i = 0; i < 3; ++i) {
        center[i] = (bounds.minimum[i] + bounds.maximum[i]) * .5F;
        radius = std::max(radius, (bounds.maximum[i] - bounds.minimum[i]) * .5F);
    }
    const auto half_width =
        radius * .9F * static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const std::array<float, 3> right{.70710678F, 0, -.70710678F},
        up{-.40824829F, .81649658F, -.40824829F}, back{.57735027F, .57735027F, .57735027F};
    float r_center = 0, u_center = 0, b_center = 0;
    for (std::size_t i = 0; i < 3; ++i) {
        view.view_projection.columns[i * 4] = right[i] / half_width;
        view.view_projection.columns[i * 4 + 1] = -up[i] / (radius * .9F);
        view.view_projection.columns[i * 4 + 2] = -back[i] / (radius * 6);
        r_center += right[i] * center[i];
        u_center += up[i] * center[i];
        b_center += back[i] * center[i];
        view.camera_position[i] = center[i] + back[i] * radius * 3;
    }
    view.view_projection.columns[12] = -r_center / half_width;
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
    {
        constexpr shader::Matrix identity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
        std::array<shader::Vertex, 3> marker{};
        const std::array<std::array<float, 2>, 3> corners{
            {{-.96F, -.82F}, {-.88F, -.82F}, {-.92F, -.70F}}};
        for (std::size_t i = 0; i < marker.size(); ++i) {
            for (std::size_t axis = 0; axis < 3; ++axis)
                marker[i].position[axis] = center[axis] + right[axis] * corners[i][0] * half_width -
                                           up[axis] * corners[i][1] * radius * .9F;
            marker[i].normal = back;
            marker[i].tangent = {right[0], right[1], right[2], 1};
        }
        const std::array<std::uint32_t, 3> indices{0, 1, 2};
        auto marker_vertices = take(context.create_buffer(
            {sizeof(marker), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, MemoryAccess::upload}));
        auto marker_indices = take(context.create_buffer(
            {sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, MemoryAccess::upload}));
        (void)take(marker_vertices.write(0, std::as_bytes(std::span(marker))));
        (void)take(marker_indices.write(0, std::as_bytes(std::span(indices))));
        MeshDraw extra{};
        extra.frame.model = extra.frame.normalMatrix = identity;
        extra.material.baseColor = {0, 0, 0, 1};
        extra.material.emissiveAlphaCutoff = {4, 0, 4, .5F};
        extra.material.flags[2] = 1;
        extra.maps.fill(&white);
        extra.index_count = 3;
        extra.facing = MeshFacing::both;
        extra.vertices = &marker_vertices;
        extra.indices = &marker_indices;
        // Camera/light matrices deliberately remain zero: the scene view must apply.
        const auto extra_submission =
            take(renderer.draw(context, version, view, targets, {}, std::span{&extra, 1}));
        wait(context, extra_submission);
        require(take(renderer.collect(context)) == 0, "completed extra scene pin did not retire");
        const auto marker_bytes = VkDeviceSize{extent.width} * extent.height * 8;
        auto marker_readback = take(context.create_buffer(
            {marker_bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAccess::readback}));
        wait(context, take(context.copy_from_image({color, marker_readback})));
        std::vector<std::uint16_t> marker_pixels(std::size_t{extent.width} * extent.height * 4);
        (void)take(marker_readback.read(0, std::as_writable_bytes(std::span(marker_pixels))));
        const auto marker_x = extent.width * 4 / 100, marker_y = extent.height * 12 / 100;
        const auto at = (std::size_t{marker_y} * extent.width + marker_x) * 4;
        require(marker_pixels[at] >= 0x4400 && marker_pixels[at + 2] >= 0x4400,
                "scene extra geometry/view was not submitted");
        std::puts("scene_extra_geometry=1 extra_scene_view=1 extra_frame_pin=1");
    }
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
    const auto bytes = VkDeviceSize{extent.width} * extent.height * 8;
    auto readback = take(
        context.create_buffer({bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAccess::readback}));
    wait(context, take(context.copy_from_image({color, readback})));
    std::vector<std::uint16_t> actual(std::size_t{extent.width} * extent.height * 4);
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
    if (!capture.empty()) {
        (void)take(ngen::probe::capture(context, color, extent, capture));
        require(SDL_SetWindowSize(context.window(), static_cast<int>(extent.width),
                                  static_cast<int>(extent.height)),
                "scene target-resolution window resize failed");
    }
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
    std::uint64_t triangles = 0;
    for (const auto &instance : package->instances) {
        const auto &lods = package->primitives[instance.primitive].lods;
        triangles += lods[std::min(std::size_t{view.lod}, lods.size() - 1)].index_count / 3;
    }
    const auto wsi = context.presentation_state().extent;
    std::printf("scene_hdr_width=%u scene_hdr_height=%u scene_wsi_width=%u scene_wsi_height=%u "
                "scene_capture=%u scene_lod=%u scene_triangles=%llu scene_draws=%zu "
                "scene_primitives=%zu scene_materials=%zu scene_light_instances=%zu\n",
                extent.width, extent.height, wsi.width, wsi.height, capture.empty() ? 0U : 1U,
                view.lod, static_cast<unsigned long long>(triangles), package->instances.size(),
                package->primitives.size(), package->materials.size(),
                package->light_instances.size());
    std::printf("checked_scene_pbr=1 generated_ibl=1 generated_shadow=1 shadow_extent=1024 "
                "frame_version_pin=1 frame_versions=2 "
                "frame_pin_retired=1 "
                "scene_lit_pixels=%zu "
                "scene_wsi_queued=1\n",
                lit);
    return true;
}
} // namespace ngen::probe
