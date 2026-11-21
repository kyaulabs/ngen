#include "draw.hpp"
#include "effects/particles.hpp"
#if defined(NGEN_SCENE_PROBE_FLIGHT_HISTORY)
#include "flight_demo/view.hpp"
#endif
#if defined(NGEN_SCENE_PROBE_PHYSICS)
#include "physics/world.hpp"
#endif
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
float half_value(std::uint16_t encoded) {
    const auto bits = static_cast<unsigned>(encoded);
    const auto exponent = static_cast<int>((bits >> 10U) & 31U);
    if (exponent == 31)
        return std::numeric_limits<float>::quiet_NaN();
    const auto significand = static_cast<float>(bits & 1023U);
    const auto value = exponent == 0 ? std::ldexp(significand, -24)
                                     : std::ldexp(1 + significand / 1024, exponent - 15);
    return (bits & 0x8000U) != 0 ? -value : value;
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
std::size_t count_lit_pixels(Context &context, const Image &color, VkExtent2D extent) {
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
    return lit;
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
                const std::filesystem::path &capture, bool particle_fixture) {
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
    constexpr VkExtent2D shadow_extent{1024, 1024};
    auto shadow_usage = static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
#if defined(NGEN_SCENE_PROBE_FLIGHT_HISTORY)
    if (particle_fixture)
        shadow_usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
#endif
    auto shadow = take(context.create_image({shadow_extent, VK_FORMAT_D32_SFLOAT, shadow_usage}));
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
    if (!active) {
        wait(context, take(renderer.draw(context, version, view, targets)));
        const auto lit = count_lit_pixels(context, color, extent);
        require(lit > 0, "single scene draw produced no lit pixels");
        std::printf("single_scene_lit_pixels=%zu\n", lit);
        return true;
    }
    if (particle_fixture) {
        auto aliased = targets;
        aliased.motion_vectors = &color;
        require(std::holds_alternative<Failure>(renderer.draw(context, version, view, aliased)),
                "motion output must reject aliasing the color attachment");
    }
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
        const auto read_pixels = [&](const Image &source) {
            wait(context, take(context.copy_from_image({source, marker_readback})));
            std::vector<std::uint16_t> pixels(std::size_t{extent.width} * extent.height * 4);
            (void)take(marker_readback.read(0, std::as_writable_bytes(std::span(pixels))));
            return pixels;
        };
        const auto marker_pixels = read_pixels(color);
        const auto marker_x = extent.width * 4 / 100, marker_y = extent.height * 12 / 100;
        const auto at = (std::size_t{marker_y} * extent.width + marker_x) * 4;
        require(marker_pixels[at] >= 0x4400 && marker_pixels[at + 2] >= 0x4400,
                "scene extra geometry/view was not submitted");
        std::puts("scene_extra_geometry=1 extra_scene_view=1 extra_frame_pin=1");
        if (particle_fixture) {
            auto motion = take(context.create_image(
                {extent, VK_FORMAT_R16G16B16A16_SFLOAT,
                 static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT}));
            auto motion_targets = targets;
            motion_targets.motion_vectors = &motion;
            const auto draw_motion = [&] {
                wait(context, take(renderer.draw(context, version, view, motion_targets, {},
                                                 std::span{&extra, 1})));
                return read_pixels(motion);
            };
            const auto invalid_motion = draw_motion();
            require(std::all_of(invalid_motion.begin(), invalid_motion.end(),
                                [](auto v) { return v == 0; }),
                    "invalid motion history did not clear the attachment");
            MeshHistory history{view.view_projection};
            extra.history = &history;
            auto stationary = draw_motion();
            require(stationary[at + 3] == 0x3c00 &&
                        std::hypot(half_value(stationary[at]), half_value(stationary[at + 1])) <=
                            .25F,
                    "stationary geometry did not publish valid zero motion");
            history.clip_from_local.columns[12] -= 16.0F / static_cast<float>(extent.width);
            history.clip_from_local.columns[13] += 8.0F / static_cast<float>(extent.height);
            const auto motion_pixels = draw_motion();
            const auto dx = half_value(motion_pixels[at]), dy = half_value(motion_pixels[at + 1]);
            const auto motion_error = std::hypot(dx + 8, dy - 4);
            require(motion_error <= .25F && motion_pixels[at + 2] == 0 &&
                        motion_pixels[at + 3] == 0x3c00,
                    "mesh motion readback differs from -8,+4 pixels");
            require(read_pixels(color) == marker_pixels,
                    "motion attachment changed the color draw");
            extra.material.flags[0] = 1;
            extra.material.baseColor[3] = 0;
            const auto masked_motion = draw_motion();
            require(masked_motion[at + 3] == 0, "alpha-masked geometry published invalid coverage");
            extra.material.flags[0] = 0;
            extra.material.baseColor[3] = 1;
            extra.history = nullptr;
            require(draw_motion() == invalid_motion,
                    "missing history did not invalidate old vectors");
            std::printf("mesh_motion_readback=1 stationary=1 current_to_previous=%.9g,%.9g "
                        "error_px=%.9g mask=1 reset=1 color_equal=1\n",
                        static_cast<double>(dx), static_cast<double>(dy),
                        static_cast<double>(motion_error));
#if defined(NGEN_SCENE_PROBE_PHYSICS)
            const auto live_motion = [&](bool transfer) {
                constexpr coordinates::Position far{{{145'000'000'000, 0x1p31 - 128}, {}, {}}};
                constexpr coordinates::Position shifted{{{145'000'000'001, -0x1p31 + 128}, {}, {}}};
                auto world = std::get<std::unique_ptr<physics::World>>(
                    physics::World::create(transfer ? far : coordinates::Position{}));
                const auto hull = std::get<physics::BodyId>(world->add_box({{1, 1, 1}, {}, 1}));
                auto basis = std::get<coordinates::Frame>(world->frame());
                const auto before = std::get<physics::BodyState>(world->state(hull));
                if (transfer) {
                    const auto result = world->rebase(before.frame, shifted);
                    require(std::holds_alternative<coordinates::Frame>(result) &&
                                std::get<coordinates::Frame>(result).stamp.epoch ==
                                    before.frame.epoch + 1,
                            "motion fixture did not perform a live Jolt rebase");
                }
                require(std::holds_alternative<std::monostate>(world->apply(hull, {360, 0, 0})) &&
                            world->step() == Result<Tick>{Tick{1}},
                        "motion fixture body update failed");
                const auto after = std::get<physics::BodyState>(world->state(hull));
                basis.stamp.tick = after.tick;
                const auto source = std::get<coordinates::Frame>(world->frame());
                const auto current = std::get<coordinates::Sample>(coordinates::transform(
                    {after.frame, after.position_metres, {}}, source, basis));
                require(std::abs(current.position_metres[0] - .1) < 1e-6,
                        "motion fixture did not follow independently expected physical motion");
                const std::array past{identity};
                shader::InstanceTransform instance{identity, identity};
                instance.model.columns[12] = static_cast<float>(current.position_metres[0]);
                const MeshHistory live_history{view.view_projection, past};
                auto live_draw = extra;
                live_draw.instances = std::span{&instance, 1};
                live_draw.history = &live_history;
                wait(context, take(renderer.draw(context, version, view, motion_targets, {},
                                                 std::span{&live_draw, 1})));
                return read_pixels(motion);
            };
            const auto fixed_motion = live_motion(false);
            const auto transferred_motion = live_motion(true);
            const auto expected_x =
                -.1F * view.view_projection.columns[0] * static_cast<float>(extent.width) * .5F;
            const auto expected_y =
                -.1F * view.view_projection.columns[1] * static_cast<float>(extent.height) * .5F;
            require(std::hypot(expected_x, expected_y) > 1,
                    "live motion fixture lacks significant screen-space motion");
            std::size_t occupied = 0;
            float peak_error = 0;
            for (std::size_t i = 0; i < transferred_motion.size(); i += 4) {
                if (transferred_motion[i + 3] == 0)
                    continue;
                require(transferred_motion[i + 3] == 0x3c00, "invalid motion validity value");
                const auto error = std::hypot(half_value(transferred_motion[i]) - expected_x,
                                              half_value(transferred_motion[i + 1]) - expected_y);
                require(error <= .25F, "live rebased motion exceeds the pixel gate");
                peak_error = std::max(peak_error, error);
                ++occupied;
            }
            require(occupied > 0 && transferred_motion == fixed_motion,
                    "live transferred and control motion attachments differ");
            std::printf("mesh_motion_live_rebase_equal=1 valid_pixels=%zu peak_error_px=%.9g\n",
                        occupied, static_cast<double>(peak_error));
#endif
#if defined(NGEN_SCENE_PROBE_FLIGHT_HISTORY)
            demo::ShipProfile profile;
            profile.flight.hull.half_extent_metres = {1, 1, 1};
            for (std::size_t axis = 0; axis < 3; ++axis)
                profile.asset_to_body.columns[12 + axis] = -center[axis];
            const auto preset = take(demo::make_preset(package, profile));
            const auto shadow_bytes = VkDeviceSize{shadow_extent.width} * shadow_extent.height * 4;
            auto shadow_readback = take(context.create_buffer(
                {shadow_bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAccess::readback}));
            struct ViewPixels {
                std::vector<std::uint16_t> color, motion;
                std::vector<float> shadow;
            };
            const auto history_pixels = [&](bool transfer) {
                constexpr coordinates::Position far{{{145'000'000'000, 0x1p31 - 128}, {}, {}}};
                constexpr coordinates::Position shifted{{{145'000'000'001, -0x1p31 + 128}, {}, {}}};
                auto flight = std::get<std::unique_ptr<flight::Flight>>(flight::Flight::create(
                    preset.ship.flight, preset.contacts, transfer ? far : coordinates::Position{}));
                auto camera =
                    std::get<flight::CameraRig>(flight::CameraRig::create(preset.ship_radius));
                auto initial = flight->snapshot();
                const auto basis = initial.frame;
                auto particles = std::get<effects::Particles>(effects::Particles::create(basis));
                require(std::holds_alternative<std::monostate>(
                            particles.emit({basis.stamp, {-1.5, 0, 0}, {-1, 0, 0}}, 32)) &&
                            std::holds_alternative<std::monostate>(flight->submit(
                                {Tick{1}, 1, initial.entity, flight::Destination{{2, 0, 0}}})),
                        "combined flight history inputs rejected");
                for (std::uint64_t tick = 1; tick <= 12; ++tick) {
                    initial = std::get<flight::Snapshot>(flight->step());
                    require(particles.step() == Result<Tick>{Tick{tick}},
                            "combined particle tick mismatch");
                }
                const auto sample = [&](const flight::Snapshot &before,
                                        const flight::Snapshot &after) {
                    return take(
                        demo::present(preset,
                                      std::get<flight::CameraFrame>(camera.sample(
                                          {before, after, &basis}, .5F, preset.camera_obstacles)),
                                      extent));
                };
                const auto prior = sample(initial, initial);
                if (transfer) {
                    const auto rebase =
                        std::get<flight::Snapshot>(flight->rebase(initial.frame.stamp, shifted));
                    require(
                        rebase.frame.stamp.epoch == initial.frame.stamp.epoch + 1 &&
                            std::holds_alternative<std::monostate>(particles.rebase(rebase.frame)),
                        "combined rebase did not move both live histories");
                }
                const auto current = std::get<flight::Snapshot>(flight->step());
                require(particles.step() == Result<Tick>{current.tick} &&
                            std::holds_alternative<std::monostate>(camera.orbit({.12F, .04F, 0})),
                        "combined camera/particle update failed");
                auto frame = sample(initial, current);
                const SceneHistory history{prior.scene.view_projection, prior.scene.root_transform};
                frame.scene.history = &history;
                const auto particle_view = particles.snapshot();
                require(particle_view.frame.stamp == current.frame.stamp &&
                            particle_view.particles().size() == 1,
                        "combined particle and flight stamps differ");
                auto target_basis = basis;
                target_basis.stamp.tick = current.tick;
                const auto position = std::get<coordinates::Sample>(coordinates::transform(
                    {particle_view.frame.stamp, particle_view.particles().front().position_metres},
                    particle_view.frame, target_basis));
                auto particle = extra;
                particle.frame.model = identity;
                for (std::size_t axis = 0; axis < 3; ++axis)
                    particle.frame.model.columns[12 + axis] = static_cast<float>(
                        position.position_metres[axis] - frame.origin_metres[axis] - center[axis]);
                wait(context, take(renderer.draw(context, version, frame.scene, motion_targets, {},
                                                 std::span{&particle, 1})));
                ViewPixels pixels{read_pixels(color), read_pixels(motion),
                                  std::vector<float>(shadow_bytes / 4)};
                wait(context, take(context.copy_from_image({shadow, shadow_readback})));
                (void)take(
                    shadow_readback.read(0, std::as_writable_bytes(std::span(pixels.shadow))));
                return pixels;
            };
            const auto control_views = history_pixels(false);
            const auto shifted_views = history_pixels(true);
            require(control_views.color == shifted_views.color &&
                        std::ranges::equal(std::as_bytes(std::span(control_views.shadow)),
                                           std::as_bytes(std::span(shifted_views.shadow))) &&
                        control_views.motion == shifted_views.motion,
                    "combined near/far HDR, shadows or GPU vectors differ");
            require(std::ranges::any_of(control_views.shadow, [](auto z) { return z < 1; }),
                    "combined shadow pass did not write depth");
            std::size_t motion_count = 0, particle_count = 0;
            float largest_motion = 0;
            for (std::size_t i = 0; i < control_views.motion.size(); i += 4) {
                if (control_views.motion[i + 3] == 0x3c00) {
                    ++motion_count;
                    const auto length = std::hypot(half_value(control_views.motion[i]),
                                                   half_value(control_views.motion[i + 1]));
                    largest_motion = std::max(largest_motion, length);
                }
                if (control_views.color[i] >= 0x4400 && control_views.color[i + 2] >= 0x4400 &&
                    control_views.motion[i + 3] == 0)
                    ++particle_count;
            }
            require(motion_count > 0 && particle_count > 0 && largest_motion > .25F,
                    "combined fixture lacks visible moving surfaces or invalid particle pixels");
            std::printf("flight_history_gpu_equal=1 valid_motion_pixels=%zu particle_pixels=%zu "
                        "control_peak_px=%.9g\n",
                        motion_count, particle_count, static_cast<double>(largest_motion));
#endif
            const VkExtent2D smaller{extent.width / 2, extent.height / 2};
            auto small_color = take(context.create_image(
                {smaller, VK_FORMAT_R16G16B16A16_SFLOAT,
                 static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                     VK_IMAGE_USAGE_SAMPLED_BIT}));
            auto small_depth = take(context.create_image(
                {smaller, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT}));
            SceneTargets resized{small_color,    small_depth,    shadow, environment[0],
                                 environment[1], environment[2], white,  &motion};
            require(std::holds_alternative<Failure>(renderer.draw(context, version, view, resized)),
                    "resized draw accepted the old motion extent");
            auto small_motion = take(context.create_image(
                {smaller, VK_FORMAT_R16G16B16A16_SFLOAT,
                 static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                     VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT}));
            resized.motion_vectors = &small_motion;
            wait(context, take(renderer.draw(context, version, view, resized)));
            const auto resized_pixels = read_pixels(small_motion);
            const auto resized_count = std::size_t{smaller.width} * smaller.height * 4;
            require(std::ranges::all_of(std::span(resized_pixels).first(resized_count),
                                        [](auto v) { return v == 0; }),
                    "new motion image after resize was not invalidated");
            std::puts("mesh_motion_resize_invalid=1");
            wait(context, take(renderer.draw(context, version, view, targets)));
            const auto plain = read_pixels(color);
            const coordinates::Frame basis{.stamp = {1, 1, {}},
                                           .origin = {{{145'000'000'000, 0x1p31 - 128}, {}, {}}}};
            auto control = std::get<effects::Particles>(effects::Particles::create(basis));
            auto shifted = std::get<effects::Particles>(effects::Particles::create(basis));
            for (auto *pool : {&control, &shifted})
                require(std::holds_alternative<std::monostate>(
                            pool->emit({basis.stamp, {}, {radius * 6.0, 0, 0}}, 4)),
                        "particle emission failed");
            auto target = basis;
            ++target.stamp.epoch;
            target.origin[0] = {145'000'000'001, -0x1p31 + 128};
            require(std::holds_alternative<std::monostate>(shifted.rebase(target)),
                    "particle origin transfer failed");
            for (auto *pool : {&control, &shifted})
                require(pool->step() == Result<Tick>{Tick{1}}, "particle tick failed");
            const auto render_particles = [&](const effects::ParticleSnapshot &snapshot) {
                const auto particles = snapshot.particles();
                require(particles.size() == 1, "unexpected particle count");
                auto output_basis = basis;
                output_basis.stamp.tick = snapshot.frame.stamp.tick;
                const auto local = std::get<coordinates::Sample>(
                    coordinates::transform({snapshot.frame.stamp, particles.front().position_metres,
                                            particles.front().velocity_mps},
                                           snapshot.frame, output_basis));
                shader::InstanceTransform instance{identity, identity};
                for (std::size_t axis = 0; axis < 3; ++axis)
                    instance.model.columns[12 + axis] =
                        static_cast<float>(local.position_metres[axis]);
                auto draw = extra;
                draw.instances = std::span{&instance, 1};
                wait(context,
                     take(renderer.draw(context, version, view, targets, {}, std::span{&draw, 1})));
                return read_pixels(color);
            };
            const auto moved = render_particles(control.snapshot());
            const auto rebased = render_particles(shifted.snapshot());
            std::size_t lit_particles = 0;
            for (std::size_t i = 0; i < moved.size(); i += 4)
                if (moved[i] >= 0x4400 && moved[i + 2] >= 0x4400)
                    ++lit_particles;
            require(lit_particles > 0 && moved != marker_pixels && rebased == moved,
                    "particle motion or origin-transfer HDR readback differs");
            wait(context, take(renderer.draw(context, version, view, targets)));
            require(read_pixels(color) == plain, "particle draw changed the default scene output");
            std::printf("particle_rebase_hdr_equal=1 particle_lit_pixels=%zu default_hdr_equal=1\n",
                        lit_particles);
        }
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
    const auto lit = count_lit_pixels(context, color, extent);
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
