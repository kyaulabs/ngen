#include <Jolt/Jolt.h>

#include "audio/output.hpp"
#include "flight/input.hpp"
#include "flight_demo/app.hpp"
#include "flight_demo/view.hpp"
#include "probe/capture.hpp"
#include "render/environment.hpp"
#include "scene_workload/allocations.hpp"
#include "tools/gui.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <imgui.h>
#include <numbers>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace ngen::demo {
namespace {
using namespace render;
using Clock = std::chrono::steady_clock;
constexpr shader::Matrix identity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class T> T take(std::variant<T, Failure> result) {
    if (const auto *error = std::get_if<Failure>(&result))
        throw std::runtime_error(error->message);
    return std::get<T>(std::move(result));
}
template <class T> T game_value(Result<T> result) {
    if (std::holds_alternative<Error>(result))
        throw std::runtime_error("flight domain rejected its authored state");
    return std::get<T>(std::move(result));
}
std::uint64_t nanoseconds(Clock::time_point start) {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - start).count());
}
void startup_events() {
    SDL_Event event{};
    for (unsigned i = 0; i < 256 && SDL_PollEvent(&event); ++i)
        require(event.type != SDL_EVENT_QUIT && event.type != SDL_EVENT_WINDOW_CLOSE_REQUESTED,
                "flight startup cancelled");
}
void wait(Context &context, const Submission &submission) {
    const auto start = SDL_GetTicks();
    while (!take(context.submission_complete(submission))) {
        require(SDL_GetTicks() - start < 10000, "flight GPU completion timed out");
        startup_events();
        SDL_Delay(1);
    }
}
Buffer buffer(Context &context, std::span<const std::byte> bytes, VkBufferUsageFlags usage) {
    auto device = take(context.create_buffer(
        {bytes.size(), usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAccess::device}));
    auto staging = take(context.create_buffer(
        {bytes.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAccess::upload}));
    (void)take(staging.write(0, bytes));
    wait(context, take(context.copy_buffer({staging, device, bytes.size()})));
    return device;
}
void upload(Context &context, const Image &image, std::span<const std::byte> bytes) {
    auto staging = take(context.create_buffer(
        {bytes.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAccess::upload}));
    (void)take(staging.write(0, bytes));
    wait(context, take(context.copy_to_image({staging, image})));
}
ScenePin resident(Context &context, assets::PackagePin package) {
    auto uploading = take(SceneUpload::begin(context, std::move(package)));
    const auto start = SDL_GetTicks();
    for (;;) {
        auto ready = take(uploading->advance(context));
        if (ready)
            return std::move(*ready);
        require(SDL_GetTicks() - start < 60000, "flight asset upload timed out");
        startup_events();
        SDL_Delay(1);
    }
}
std::optional<flight::FlightKey> mapped(SDL_Scancode key) {
    using K = flight::FlightKey;
    switch (key) {
    case SDL_SCANCODE_W:
        return K::forward;
    case SDL_SCANCODE_S:
        return K::backward;
    case SDL_SCANCODE_A:
        return K::left;
    case SDL_SCANCODE_D:
        return K::right;
    case SDL_SCANCODE_LSHIFT:
        return K::up;
    case SDL_SCANCODE_LCTRL:
        return K::down;
    case SDL_SCANCODE_UP:
        return K::pitch_up;
    case SDL_SCANCODE_DOWN:
        return K::pitch_down;
    case SDL_SCANCODE_LEFT:
        return K::yaw_left;
    case SDL_SCANCODE_RIGHT:
        return K::yaw_right;
    case SDL_SCANCODE_Q:
        return K::roll_left;
    case SDL_SCANCODE_E:
        return K::roll_right;
    case SDL_SCANCODE_SPACE:
        return K::brake;
    case SDL_SCANCODE_LALT:
        return K::free_look;
    default:
        return {};
    }
}
struct Ray {
    JPH::DVec3 origin, direction;
};
Ray ray(const flight::CameraFrame &camera, std::array<float, 2> pixel, VkExtent2D extent) {
    const auto scale = std::tan(camera.vertical_fov_degrees * std::numbers::pi_v<float> / 360);
    const auto x = (2 * pixel[0] / static_cast<float>(extent.width) - 1) * scale *
                   static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const auto y = -(2 * pixel[1] / static_cast<float>(extent.height) - 1) * scale;
    const auto &q = camera.camera.rotation_xyzw;
    const JPH::Quat rotation(q[0], q[1], q[2], q[3]);
    const auto direction = rotation * JPH::Vec3{x, y, -1}.Normalized();
    const auto &p = camera.camera.position_metres;
    return {{p[0], p[1], p[2]}, JPH::DVec3(direction)};
}
std::optional<std::size_t> pick(const Ray &ray, const Preset &preset) {
    double nearest = 8192;
    std::optional<std::size_t> selected;
    for (std::size_t i = 0; i < preset.camera_obstacles.size(); ++i) {
        const auto &sphere = preset.camera_obstacles[i];
        const auto &p = sphere.centre_metres;
        const auto delta = ray.origin - JPH::DVec3{p[0], p[1], p[2]};
        const auto b = delta.Dot(ray.direction);
        const auto discriminant =
            b * b - delta.LengthSq() + sphere.radius_metres * sphere.radius_metres;
        if (discriminant >= 0) {
            const auto distance = -b - std::sqrt(discriminant);
            if (distance > 0 && distance < nearest) {
                nearest = distance;
                selected = i;
            }
        }
    }
    return selected;
}
struct Sample {
    std::uint64_t frame_ns{}, host_ns{};
    flight::Snapshot state;
    flight::CameraFrame camera;
    audio::Statistics audio;
    Submission mesh, post;
    GpuTimings gpu;
    ResourceStatistics memory;
    VkExtent2D extent{};
    float phase{}, power{};
    bool gpu_ready{}, focused{}, capture_requested{}, capture_accepted{};
};
struct Hud {
    std::array<float, 240> speed{}, force{};
    std::size_t cursor{};
    double frame_ms{};
    GpuTimings gpu;
    ResourceStatistics memory;
    audio::Statistics audio;
    const char *status{"Select an asteroid, or click the flight plane to travel"};
    bool help{};
};
struct Actions {
    bool mode{}, reset{}, approach{}, orbit{}, focus{}, mute{};
};
Actions hud(tools::Gui &gui, const flight::Snapshot &state, const Preset &preset, Hud &metrics,
            std::optional<std::size_t> selected, bool replay, bool mute, const FrameView &view) {
    gui.begin();
    Actions action;
    const auto size = ImGui::GetIO().DisplaySize;
    constexpr auto flags =
        static_cast<ImGuiWindowFlags>(static_cast<unsigned>(ImGuiWindowFlags_NoDecoration) |
                                      static_cast<unsigned>(ImGuiWindowFlags_NoSavedSettings) |
                                      static_cast<unsigned>(ImGuiWindowFlags_NoMove) |
                                      static_cast<unsigned>(ImGuiWindowFlags_NoNav));
    ImGui::SetNextWindowPos({16, 12});
    ImGui::SetNextWindowSize({std::max(320.0F, size.x - 32), 42});
    if (ImGui::Begin("Flight header", nullptr, flags)) {
        ImGui::TextUnformatted("NGEN  /  OFFLINE FLIGHT");
        ImGui::SameLine(285);
        ImGui::TextUnformatted(state.mode == flight::Mode::tactical
                                   ? "TACTICAL / ORBIT VIEW"
                                   : "DIRECT / AUTHORED BOW SENSOR");
        ImGui::SameLine(std::max(650.0F, size.x - 425));
        ImGui::Text("%s  |  60 Hz", replay ? "FIXED-TICK REPLAY" : "LOCAL AUTHORITY");
        ImGui::SameLine(std::max(900.0F, size.x - 160));
        action.mute = ImGui::Button(mute ? "Unmute [M]" : "Mute [M]", {112, 25});
    }
    ImGui::End();
    ImGui::SetNextWindowPos({16, 70});
    ImGui::SetNextWindowSize({252, std::min(550.0F, std::max(300.0F, size.y - 150))});
    if (ImGui::Begin("Flight telemetry", nullptr, flags)) {
        const auto &v = state.body.velocity_mps;
        const auto speed = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        ImGui::TextUnformatted("LIGHT FRIGATE / LOCAL CONTACTS");
        ImGui::Separator();
        ImGui::Text("SPEED     %6.2f m/s", speed);
        ImGui::Text("MASS      %6.1f kt", state.body.mass_kg / 1e6);
        ImGui::Text("X %8.1f  Y %8.1f\nZ %8.1f m", state.body.position_metres[0],
                    state.body.position_metres[1], state.body.position_metres[2]);
        ImGui::TextDisabled("100 asteroid proxies / 101 bodies");
        ImGui::BeginDisabled(replay);
        action.mode = ImGui::Button("Switch view [TAB]", {-1, 26});
        action.reset = ImGui::Button("Reset scene [R]", {-1, 26});
        if (selected) {
            ImGui::Text("TARGET / ASTEROID %03u", static_cast<unsigned>(*selected));
            action.approach = ImGui::Button("Approach [G]", {112, 25});
            ImGui::SameLine();
            action.orbit = ImGui::Button("Orbit [O]", {112, 25});
            action.focus = ImGui::Button("Focus / ship [F]", {-1, 25});
        } else
            ImGui::TextDisabled("Left-click a rock to select");
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::PlotLines("##speed", metrics.speed.data(), static_cast<int>(metrics.speed.size()),
                         static_cast<int>(metrics.cursor), "SPEED / m/s", 0, 100, {228, 68});
        ImGui::PlotLines("##force", metrics.force.data(), static_cast<int>(metrics.force.size()),
                         static_cast<int>(metrics.cursor), "FORCE / MN", 0,
                         preset.ship.flight.maximum_force_newtons[2] / 1e6F, {228, 68});
        ImGui::Text(
            "CPU %.2f | GPU passes %.2f ms", metrics.frame_ms,
            (metrics.gpu.shadow_ns + metrics.gpu.pbr_ns + metrics.gpu.post_ns + metrics.gpu.ui_ns) /
                1e6);
        ImGui::TextDisabled("VMA %.1f MiB (not whole GPU)",
                            static_cast<double>(metrics.memory.allocation_bytes) / (1024 * 1024));
    }
    ImGui::End();
    ImGui::SetNextWindowPos({16, std::max(360.0F, size.y) - 62});
    ImGui::SetNextWindowSize({std::max(320.0F, size.x - 32), 48});
    if (ImGui::Begin("Flight bindings", nullptr, flags)) {
        ImGui::TextUnformatted("WASD thrust | SHIFT/CTRL vertical | QE roll | SPACE brake | RMB "
                               "steer/orbit | ALT+RMB look | F1 help");
        ImGui::TextDisabled("AUDIO %s / M | %s",
                            metrics.audio.state == audio::State::disconnected
                                ? "OFFLINE (retrying)"
                                : (metrics.audio.muted ? "MUTED" : "ACTIVE"),
                            metrics.status);
    }
    ImGui::End();
    if (metrics.help) {
        ImGui::SetNextWindowPos({std::max(280.0F, size.x - 430), 80});
        ImGui::SetNextWindowSize({410, 260});
        if (ImGui::Begin("Flight help", &metrics.help, ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextWrapped(
                "TACTICAL: right-drag orbits; wheel zooms. Left-click a rock to select, or empty "
                "flight plane to travel. G approaches; O orbits the selected rock; F focuses it.");
            ImGui::Separator();
            ImGui::TextWrapped("DIRECT: hold right mouse to capture a virtual steering stick. Move "
                               "back to center or release to stabilize. Hold LEFT ALT for "
                               "camera-only free look. Arrow keys also steer; Q/E roll.");
            ImGui::Separator();
            ImGui::TextWrapped(
                "W/S forward/back, A/D lateral, SHIFT/CTRL vertical. Release thrust to coast; hold "
                "SPACE to brake. TAB changes mode without resetting motion. R resets the preset; "
                "ESC releases controls. Closing the window exits.");
            ImGui::TextWrapped("M toggles audio. Focus loss silences playback. Device loss retries "
                               "without changing simulation; original cues use no media files.");
        }
        ImGui::End();
    }
    auto *draw = ImGui::GetForegroundDrawList();
    const ImVec2 center{size.x * .5F, size.y * .5F};
    draw->AddLine({center.x - 7, center.y}, {center.x + 7, center.y}, IM_COL32(190, 225, 230, 180));
    draw->AddLine({center.x, center.y - 7}, {center.x, center.y + 7}, IM_COL32(190, 225, 230, 180));
    if (selected) {
        const auto &point = preset.camera_obstacles[*selected].centre_metres;
        if (const auto screen = project_target(view, point, {size.x, size.y}))
            draw->AddCircle({(*screen)[0], (*screen)[1]}, 12, IM_COL32(110, 215, 230, 230), 24,
                            1.5F);
    }
    gui.end();
    return action;
}
} // namespace
void run(const assets::PackagePin &package, const Preset &preset, const Options &options) {
    const bool fixed_tick = options.replay || options.walkthrough;
    auto game = game_value(flight::Flight::create(preset.ship.flight, preset.contacts));
    auto current = game->snapshot(), previous = current;
    auto rig = game_value(flight::CameraRig::create(preset.ship_radius));
    flight::FlightInput input;
    auto context = take(Context::create({options.validation, true, true, true}));
    require(SDL_SetWindowTitle(context->window(), "NGEN / Offline Flight"),
            "flight title update failed");
    require(SDL_SetWindowSize(context->window(), static_cast<int>(options.extent.width),
                              static_cast<int>(options.extent.height)),
            "flight window size failed");
    auto version = resident(*context, package);
    auto vertices = buffer(*context, std::as_bytes(std::span(preset.field.vertices)),
                           VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
    auto indices = buffer(*context, std::as_bytes(std::span(preset.field.indices)),
                          VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    auto color = take(
        context->create_image({options.extent, VK_FORMAT_R16G16B16A16_SFLOAT,
                               static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT}));
    auto depth = take(context->create_image(
        {options.extent, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT}));
    auto shadow = take(context->create_image(
        {{1024, 1024},
         VK_FORMAT_D32_SFLOAT,
         static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) |
             VK_IMAGE_USAGE_SAMPLED_BIT}));
    auto white = take(context->create_image({{1, 1}, VK_FORMAT_R8G8B8A8_UNORM}));
    const std::array<std::byte, 4> white_pixel{std::byte{255}, std::byte{255}, std::byte{255},
                                               std::byte{255}};
    upload(*context, white, white_pixel);
    const auto sky = take(generate_environment(Sky{}));
    std::array<Image, 3> environment;
    for (std::size_t i = 0; i < environment.size(); ++i) {
        environment[i] = take(context->create_image(sky.images[i].description));
        upload(*context, environment[i], sky.images[i].texels);
    }
    const SceneTargets targets{color,          depth,          shadow, environment[0],
                               environment[1], environment[2], white};
    MeshDraw asteroids{};
    asteroids.frame.model = asteroids.frame.normalMatrix = identity;
    asteroids.material.baseColor = {.23F, .19F, .15F, 1};
    asteroids.material.metallicRoughnessNormalOcclusion = {.05F, .95F, 1, 1};
    asteroids.material.flags[2] = 1;
    asteroids.maps.fill(&white);
    asteroids.index_count = static_cast<std::uint32_t>(preset.field.indices.size());
    asteroids.vertices = &vertices;
    asteroids.indices = &indices;
    SceneRenderer renderer;
    auto gui = take(tools::Gui::create(*context));
    audio::Output sound;
    bool muted = options.mute;
    (void)sound.mute(muted);
    Hud metrics;
    bool quit = false, captured = false, reset_requested = false;
    bool focused = (SDL_GetWindowFlags(context->window()) & SDL_WINDOW_INPUT_FOCUS) != 0;
    input.focus(focused);
    std::optional<std::size_t> selected, camera_target;
    std::uint64_t sequence = 0, impact_after_tick = 0;
    auto pending_mode = flight::Mode::tactical;
    auto last_camera = game_value(rig.sample({previous, current}, 1, preset.camera_obstacles));
    std::array<assets::MaterialFactors, 256> materials{};
    std::array<EmissiveFlow, 256> flows{};
    for (std::size_t i = 0; i < package->materials.size(); ++i) {
        const auto &m = package->materials[i];
        materials[i] = {m.base_color, m.metallic, m.roughness, m.emissive_strength};
    }
    const auto send = [&](flight::Intent intent) {
        const auto target = next(game->snapshot().tick);
        if (std::holds_alternative<Error>(target))
            return false;
        const auto result =
            game->submit({std::get<Tick>(target), ++sequence, game->snapshot().entity, intent});
        if (std::holds_alternative<Error>(result)) {
            metrics.status = "Intent rejected: envelope, generation or queue limit";
            return false;
        }
        return true;
    };
    const auto default_orbit = [&] {
        selected = 0;
        require(send(flight::Orbit{preset.camera_obstacles[0].centre_metres, 600, 20, false}),
                "authored default orbit was rejected");
        metrics.status = "Tactical orbit autopilot / 20 m/s; TAB takes direct control";
    };
    default_orbit();
    const auto change_mode = [&] {
        const auto wanted =
            pending_mode == flight::Mode::tactical ? flight::Mode::direct : flight::Mode::tactical;
        if (send(flight::SetMode{wanted})) {
            pending_mode = wanted;
            (void)input.mode(wanted);
            (void)sound.cue(audio::Cue::confirm);
            metrics.status = wanted == flight::Mode::direct
                                 ? "Direct bow sensor / hold RMB for virtual-stick steering"
                                 : "Tactical coast / G or O starts fresh navigation";
        }
    };
    const auto reset = [&] {
        current = game_value(game->reset());
        previous = current;
        sequence = 0;
        (void)sound.reset();
        (void)sound.cue(audio::Cue::reset);
        impact_after_tick = 0;
        input.release();
        (void)input.mode(flight::Mode::tactical);
        pending_mode = flight::Mode::tactical;
        rig = game_value(flight::CameraRig::create(preset.ship_radius));
        camera_target.reset();
        default_orbit();
        metrics.speed.fill(0);
        metrics.force.fill(0);
        metrics.cursor = 0;
        metrics.status = "Preset reset: pose and controls restored; GPU asset remains immutable";
    };
    const auto focus = [&] {
        if (selected && camera_target != selected) {
            (void)game_value(rig.focus(preset.camera_obstacles[*selected]));
            camera_target = selected;
            metrics.status = "Camera focused on the selected asteroid; F returns to the ship";
        } else {
            (void)game_value(rig.focus(std::nullopt));
            camera_target.reset();
            metrics.status = "Camera follows the ship; physical motion is unchanged";
        }
        (void)sound.cue(audio::Cue::confirm);
    };
    const auto approach = [&] {
        if (!selected)
            return;
        const auto &target = preset.camera_obstacles[*selected];
        const auto &p = current.body.position_metres;
        JPH::DVec3 delta{p[0] - target.centre_metres[0], p[1] - target.centre_metres[1],
                         p[2] - target.centre_metres[2]};
        if (delta.LengthSq() < 1e-8)
            delta = JPH::DVec3{0, 0, 1};
        delta = delta.Normalized() * (preset.ship_radius + target.radius_metres + 30);
        if (send(flight::Destination{{target.centre_metres[0] + delta.GetX(),
                                      target.centre_metres[1] + delta.GetY(),
                                      target.centre_metres[2] + delta.GetZ()}}))
            (void)sound.cue(audio::Cue::confirm);
    };
    const auto orbit = [&] {
        if (!selected)
            return;
        const auto &target = preset.camera_obstacles[*selected];
        const auto acceleration =
            *std::min_element(preset.ship.flight.maximum_force_newtons.begin(),
                              preset.ship.flight.maximum_force_newtons.end()) /
            current.body.mass_kg;
        const auto radius = static_cast<float>(
            std::max({600.0, static_cast<double>(preset.ship_radius) * 3.5 + target.radius_metres,
                      400 / (.4 * acceleration)}));
        if (send(flight::Orbit{target.centre_metres, radius, 20, false}))
            (void)sound.cue(audio::Cue::confirm);
    };
    const auto walkthrough = [&](std::uint64_t index) {
        const auto id = SDL_GetWindowID(context->window());
        const auto push = [](SDL_Event event) {
            require(SDL_PushEvent(&event), "SDL walkthrough event admission failed");
        };
        const auto key = [&](SDL_Scancode scancode, bool down) {
            SDL_Event event{};
            event.type = down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
            event.key.windowID = id;
            event.key.scancode = scancode;
            event.key.down = down;
            push(event);
        };
        const auto button = [&](std::uint8_t which, bool down) {
            SDL_Event event{};
            event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
            event.button.windowID = id;
            event.button.button = which;
            event.button.down = down;
            event.button.x = 142;
            event.button.y = 239;
            push(event);
        };
        const auto focus_event = [&](bool focused) {
            SDL_Event event{};
            event.type = focused ? SDL_EVENT_WINDOW_FOCUS_GAINED : SDL_EVENT_WINDOW_FOCUS_LOST;
            event.window.windowID = id;
            push(event);
        };
        switch (index) {
        case 0:
            focus_event(true);
            key(SDL_SCANCODE_TAB, true);
            key(SDL_SCANCODE_TAB, false);
            key(SDL_SCANCODE_W, true);
            break;
        case 5:
        case 15:
            key(SDL_SCANCODE_F1, true);
            key(SDL_SCANCODE_F1, false);
            break;
        case 30:
            key(SDL_SCANCODE_W, false);
            break;
        case 60:
            key(SDL_SCANCODE_SPACE, true);
            break;
        case 89: {
            int width = 0, height = 0;
            require(SDL_GetWindowSize(context->window(), &width, &height),
                    "walkthrough size failed");
            SDL_Event event{};
            event.type = SDL_EVENT_MOUSE_MOTION;
            event.motion.windowID = id;
            event.motion.x = static_cast<float>(width) * .5F;
            event.motion.y = static_cast<float>(height) * .5F;
            push(event);
            break;
        }
        case 90: {
            key(SDL_SCANCODE_SPACE, false);
            SDL_Event button_event{};
            button_event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            button_event.button.windowID = id;
            button_event.button.button = SDL_BUTTON_RIGHT;
            button_event.button.down = true;
            push(button_event);
            SDL_Event motion{};
            motion.type = SDL_EVENT_MOUSE_MOTION;
            motion.motion.windowID = id;
            motion.motion.xrel = 100;
            push(motion);
            key(SDL_SCANCODE_D, true);
            break;
        }
        case 120:
            focus_event(false);
            break;
        case 150:
            focus_event(true);
            break;
        case 160:
        case 170: {
            SDL_Event event{};
            event.type =
                index == 160 ? SDL_EVENT_AUDIO_DEVICE_REMOVED : SDL_EVENT_AUDIO_DEVICE_ADDED;
            event.adevice.recording = false;
            push(event);
            break;
        }
        case 180:
            key(SDL_SCANCODE_TAB, true);
            key(SDL_SCANCODE_TAB, false);
            break;
        case 210:
        case 240:
            key(SDL_SCANCODE_F, true);
            key(SDL_SCANCODE_F, false);
            break;
        case 270:
            key(SDL_SCANCODE_R, true);
            key(SDL_SCANCODE_R, false);
            break;
        case 299: {
            SDL_Event event{};
            event.type = SDL_EVENT_MOUSE_MOTION;
            event.motion.windowID = id;
            event.motion.x = 142;
            event.motion.y = 239;
            push(event);
            break;
        }
        case 300:
            button(SDL_BUTTON_LEFT, true);
            break;
        case 301:
            button(SDL_BUTTON_LEFT, false);
            break;
        case 320:
        case 325:
            key(SDL_SCANCODE_M, true);
            key(SDL_SCANCODE_M, false);
            break;
        default:
            break;
        }
    };
    const auto pump = [&] {
        SDL_Event event{};
        for (unsigned n = 0; n < 256 && SDL_PollEvent(&event); ++n) {
            gui->event(event);
            (void)sound.event(event);
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                quit = true;
                continue;
            }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                focused = false;
                input.focus(false);
                (void)sound.mute(true);
                continue;
            }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
                focused = true;
                input.focus(true);
                continue;
            }
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                const bool down = event.type == SDL_EVENT_KEY_DOWN;
                if (!down) {
                    if (const auto key = mapped(event.key.scancode))
                        (void)input.key(*key, false);
                    continue;
                }
                if (event.key.scancode == SDL_SCANCODE_ESCAPE) {
                    input.release();
                    ImGui::SetWindowFocus(nullptr);
                    continue;
                }
                if (event.key.scancode == SDL_SCANCODE_F1 && !event.key.repeat) {
                    metrics.help = !metrics.help;
                    continue;
                }
                if (event.key.scancode == SDL_SCANCODE_M && !event.key.repeat) {
                    muted = !muted;
                    continue;
                }
                if (options.replay)
                    continue;
                if (!gui->wants_keyboard() || input.capture()) {
                    if (!event.key.repeat)
                        switch (event.key.scancode) {
                        case SDL_SCANCODE_TAB:
                            change_mode();
                            break;
                        case SDL_SCANCODE_R:
                            reset_requested = true;
                            break;
                        case SDL_SCANCODE_F:
                            focus();
                            break;
                        case SDL_SCANCODE_O:
                            orbit();
                            break;
                        case SDL_SCANCODE_G:
                            approach();
                            break;
                        default:
                            break;
                        }
                    if (const auto key = mapped(event.key.scancode))
                        (void)input.key(*key, true);
                }
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_UP &&
                       event.button.button == SDL_BUTTON_RIGHT)
                input.pointer(false);
            else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && !options.replay &&
                     !gui->wants_mouse()) {
                if (event.button.button == SDL_BUTTON_RIGHT)
                    input.pointer(true);
                else if (event.button.button == SDL_BUTTON_LEFT &&
                         pending_mode == flight::Mode::tactical) {
                    int w = 0, h = 0;
                    if (SDL_GetWindowSize(context->window(), &w, &h) && w > 0 && h > 0) {
                        const auto direction =
                            ray(last_camera, {event.button.x, event.button.y},
                                {static_cast<unsigned>(w), static_cast<unsigned>(h)});
                        selected = pick(direction, preset);
                        if (!selected && std::abs(direction.direction.GetY()) > 1e-6) {
                            const auto distance =
                                (current.body.position_metres[1] - direction.origin.GetY()) /
                                direction.direction.GetY();
                            if (distance > 0 && distance < 4096) {
                                const auto point =
                                    direction.origin + direction.direction * distance;
                                (void)send(flight::Destination{
                                    {point.GetX(), point.GetY(), point.GetZ()}});
                            }
                        }
                    }
                }
            } else if (event.type == SDL_EVENT_MOUSE_MOTION && !options.replay)
                (void)input.motion({event.motion.xrel, event.motion.yrel});
            else if (event.type == SDL_EVENT_MOUSE_WHEEL && !options.replay && !gui->wants_mouse())
                (void)input.wheel(std::clamp(event.wheel.direction == SDL_MOUSEWHEEL_FLIPPED
                                                 ? -event.wheel.y
                                                 : event.wheel.y,
                                             -16.0F, 16.0F));
        }
        if (input.capture() != captured) {
            if (SDL_SetWindowRelativeMouseMode(context->window(), input.capture()))
                captured = input.capture();
            else {
                input.pointer(false);
                captured = false;
                metrics.status = "Pointer capture unavailable; keyboard controls remain active";
            }
        }
    };
    const auto advance = [&] {
        previous = current;
        if (!options.replay)
            (void)send(input.pilot());
        const auto result = game->step();
        if (const auto *error = std::get_if<Error>(&result)) {
            require(*error == Error::invalid, "flight physics stopped; reset is required");
            metrics.status = "Combined intent rejected before physical publication";
            return;
        }
        current = std::get<flight::Snapshot>(result);
        pending_mode = current.mode;
        (void)input.mode(current.mode);
        const auto &v = current.body.velocity_mps;
        const auto &f = current.force_newtons;
        double residual_squared = 0;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const auto residual = static_cast<double>(v[axis]) - previous.body.velocity_mps[axis] -
                                  static_cast<double>(f[axis]) / previous.body.mass_kg / 60;
            residual_squared += residual * residual;
        }
        // An audible impulse estimate, not a new authoritative contact stream.
        if (residual_squared > .25 * .25 && current.tick.value >= impact_after_tick) {
            (void)sound.cue(audio::Cue::impact);
            impact_after_tick = current.tick.value + 12;
        }
        metrics.speed[metrics.cursor] = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        metrics.force[metrics.cursor] = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]) / 1e6F;
        metrics.cursor = (metrics.cursor + 1) % metrics.speed.size();
    };
    const auto scripted = [&] {
        switch (current.tick.value + 1) {
        case 1:
            (void)send(flight::Destination{{0, 0, -600}});
            break;
        case 60:
            (void)rig.orbit({.45F, 0, 0});
            break;
        case 120:
            (void)send(flight::SetMode{flight::Mode::direct});
            (void)send(flight::Pilot{{0, 0, -1}});
            break;
        case 180:
            (void)send(flight::Pilot{});
            (void)rig.look({.25F, -.1F, 0});
            break;
        case 240:
            (void)send(flight::Pilot{{}, {}, true});
            rig.center_look();
            break;
        case 300:
            (void)send(flight::SetMode{flight::Mode::tactical});
            (void)send(flight::Destination{{0, 0, -600}});
            break;
        case 600:
            selected = 0;
            focus();
            break;
        case 900:
            orbit();
            break;
        case 1800:
            (void)rig.focus(std::nullopt);
            camera_target.reset();
            (void)send(flight::SetMode{flight::Mode::direct});
            (void)send(flight::Pilot{{0, 0, -1}, {0, .3F, 0}});
            break;
        case 1980:
            (void)send(flight::Pilot{});
            // Keep the detailed hull in the benchmark after the direct-turn sample.
            (void)send(flight::SetMode{flight::Mode::tactical});
            break;
        case 2400:
            (void)send(flight::Pilot{{}, {}, true});
            break;
        default:
            break;
        }
    };
    std::vector<Sample> samples(options.frames);
    std::size_t measured = 0, collected = 0;
    Submission last;
    const auto collect = [&] {
        while (collected < measured) {
            auto &sample = samples[collected];
            if (!take(context->submission_complete(sample.post)))
                break;
            const auto mesh = take(context->submission_timings(sample.mesh));
            const auto post = take(context->submission_timings(sample.post));
            if (mesh && post) {
                sample.gpu = {mesh->shadow_ns, mesh->pbr_ns, post->post_ns, post->ui_ns};
                sample.gpu_ready = true;
                metrics.gpu = sample.gpu;
            }
            ++collected;
        }
    };
    const auto warmup = options.frames ? options.warmup : 0U;
    std::uint64_t frame_index = 0;
    double accumulator = 0;
    auto time = Clock::now();
    std::uint64_t allocation_start = 0, ui_start = 0;
    for (;;) {
        if (options.frames && measured == options.frames)
            break;
        if (quit)
            break;
        if (options.frames && frame_index == warmup) {
            reset();
            time = Clock::now();
            accumulator = 0;
            allocation_start = workload::allocation_count();
            ui_start = gui->allocation_count();
        }
        const auto began = Clock::now();
        if (options.walkthrough && frame_index >= warmup)
            walkthrough(frame_index - warmup);
        pump();
        if (quit)
            break;
        // SDL admission retries and GUI actions may request a reset, but only
        // this boundary may replace the domain before this frame is frozen.
        if (reset_requested) {
            reset();
            reset_requested = false;
            accumulator = 0;
            time = Clock::now();
        }
        if (!options.frames &&
            (SDL_GetWindowFlags(context->window()) & SDL_WINDOW_MINIMIZED) != 0) {
            input.release();
            (void)sound.mute(true);
            time = Clock::now();
            SDL_Delay(16);
            continue;
        }
        if (fixed_tick) {
            if (options.replay && frame_index >= warmup)
                scripted();
            advance();
        } else {
            const auto now = Clock::now();
            accumulator += std::min(.25, std::chrono::duration<double>(now - time).count());
            time = now;
            while (accumulator >= 1.0 / 60) {
                advance();
                accumulator -= 1.0 / 60;
            }
        }
        const auto camera_events = input.take_camera();
        if (camera_events.center_look)
            rig.center_look();
        (void)rig.orbit(camera_events.orbit);
        (void)rig.look(camera_events.look);
        auto camera = rig.sample({previous, current},
                                 fixed_tick ? 1.0F : static_cast<float>(accumulator * 60),
                                 preset.camera_obstacles);
        if (const auto *error = std::get_if<Error>(&camera); error && *error == Error::busy) {
            (void)rig.focus(std::nullopt);
            camera_target.reset();
            metrics.status = "Camera clearance: following the ship";
            camera = rig.sample({previous, current}, 1, preset.camera_obstacles);
        }
        last_camera = game_value(camera);
        int width = 0, height = 0;
        require(SDL_GetWindowSizeInPixels(context->window(), &width, &height) && width > 0 &&
                    height > 0,
                "flight drawable unavailable");
        const VkExtent2D drawable{static_cast<unsigned>(width), static_cast<unsigned>(height)};
        auto view = take(present(preset, last_camera, drawable));
        const auto action =
            hud(*gui, current, preset, metrics, selected, options.replay, muted, view);
        if (action.mute)
            muted = !muted;
        if (!options.replay) {
            if (action.mode)
                change_mode();
            if (action.reset)
                reset_requested = true;
            if (action.focus)
                focus();
            if (action.approach)
                approach();
            if (action.orbit)
                orbit();
        }
        auto overlay = take(gui->overlay(drawable));
        const auto phase =
            static_cast<float>(static_cast<std::uint32_t>(
                static_cast<float>(std::fmod(static_cast<double>(current.tick.value) / 60, 16) /
                                   16) *
                65536)) /
            65536;
        // The published world wrench was applied with the before-tick attitude.
        const auto &q = previous.body.rotation_xyzw;
        const auto &force = current.force_newtons;
        const auto local = JPH::Quat(q[0], q[1], q[2], q[3]).Conjugated() *
                           JPH::Vec3{force[0], force[1], force[2]};
        const auto power =
            std::clamp(-local.GetZ() / preset.ship.flight.maximum_force_newtons[2], 0.0F, 1.0F);
        const auto &limits = preset.ship.flight.maximum_force_newtons;
        const auto thrust = std::clamp(
            std::max({std::abs(local.GetX()) / limits[0], std::abs(local.GetY()) / limits[1],
                      std::abs(local.GetZ()) / limits[2]}),
            0.0F, 1.0F);
        const auto &velocity = current.body.velocity_mps;
        const auto speed = std::sqrt(velocity[0] * velocity[0] + velocity[1] * velocity[1] +
                                     velocity[2] * velocity[2]);
        float work = 0;
        for (std::size_t axis = 0; axis < 3; ++axis)
            work += force[axis] * previous.body.velocity_mps[axis];
        (void)sound.mute(muted || !focused);
        (void)sound.update({.35F, thrust, work < 0 ? thrust : 0,
                            std::clamp(speed / preset.ship.flight.maximum_speed_mps, 0.0F, 1.0F)});
        if (preset.drive_material) {
            const auto i = *preset.drive_material;
            materials[i].base_color = package->materials[i].base_color;
            materials[i].base_color[3] *= power;
            materials[i].emissive_strength = package->materials[i].emissive_strength * power;
            flows[i] = {phase, power > 0 ? 1.0F : 0.0F};
            view.scene.material_factors = std::span(materials).first(package->materials.size());
            view.scene.material_flows = std::span(flows).first(package->materials.size());
        }
        asteroids.instances = view.asteroids;
        const auto admission = SDL_GetTicks();
        Submission mesh;
        for (;;) {
            auto result =
                renderer.draw(*context, version, view.scene, targets, {}, std::span{&asteroids, 1});
            const auto *error = std::get_if<Failure>(&result);
            if (!error || (error->message != "transfer contexts are busy" &&
                           error->message != "scene frame pins are busy")) {
                mesh = take(std::move(result));
                break;
            }
            require(SDL_GetTicks() - admission < 10000, "flight draw admission timed out");
            (void)take(renderer.collect(*context));
            pump();
            SDL_Delay(1);
        }
        bool presented = false;
        while (!quit) {
            const auto result =
                take(context->render_frame({color, 1, true, overlay ? &*overlay : nullptr, .04F}));
            if (result.outcome == FrameOutcome::queued) {
                last = result.resources;
                presented = true;
                break;
            }
            if (!options.frames &&
                (SDL_GetWindowFlags(context->window()) & SDL_WINDOW_MINIMIZED) != 0)
                break;
            require(SDL_GetTicks() - admission < 10000, "flight presentation admission timed out");
            pump();
            SDL_Delay(1);
        }
        if (!presented)
            continue;
        metrics.memory = take(context->resource_statistics());
        metrics.audio = sound.statistics();
        if (options.frames && frame_index >= warmup) {
            auto &sample = samples[measured++];
            sample.state = current;
            sample.camera = last_camera;
            sample.audio = metrics.audio;
            sample.focused = focused;
            sample.capture_requested = input.capture();
            sample.capture_accepted = captured;
            sample.mesh = mesh;
            sample.post = last;
            sample.memory = metrics.memory;
            sample.extent = context->presentation_state().extent;
            sample.phase = phase;
            sample.power = power;
            collect();
            const auto ended = Clock::now();
            sample.frame_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(ended - began).count());
            sample.host_ns = static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(ended.time_since_epoch())
                    .count());
            metrics.frame_ms = static_cast<double>(sample.frame_ns) / 1e6;
        } else
            metrics.frame_ms = static_cast<double>(nanoseconds(began)) / 1e6;
        ++frame_index;
    }
    const auto allocations = options.frames ? workload::allocation_count() - allocation_start : 0;
    const auto ui_allocations = options.frames ? gui->allocation_count() - ui_start : 0;
    require(!options.frames || measured == options.frames,
            "flight replay cancelled before its complete frame set");
    if (frame_index) {
        wait(*context, last);
        collect();
        require(take(renderer.collect(*context)) == 0, "flight scene pins did not retire");
    }
    const auto memory = take(context->resource_statistics());
    std::size_t lit_pixels = 0;
    if (frame_index) {
        auto readback = take(
            context->create_buffer({VkDeviceSize{options.extent.width} * options.extent.height * 8,
                                    VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAccess::readback}));
        wait(*context, take(context->copy_from_image({color, readback})));
        std::vector<std::uint16_t> hdr(std::size_t{options.extent.width} * options.extent.height *
                                       4);
        (void)take(readback.read(0, std::as_writable_bytes(std::span(hdr))));
        for (std::size_t i = 0; i < hdr.size(); i += 4) {
            bool lit = false;
            for (std::size_t channel = 0; channel < 4; ++channel) {
                require((hdr[i + channel] & 0x7c00U) != 0x7c00U, "flight HDR output is nonfinite");
                lit = lit || (channel < 3 && hdr[i + channel] != 0);
            }
            lit_pixels += lit ? 1U : 0U;
        }
        require(!options.replay || lit_pixels > 0, "flight replay produced no lit world pixels");
    }
    if (!options.capture.empty() && frame_index) {
        auto overlay = take(gui->overlay(options.extent));
        (void)take(probe::capture(*context, color, options.extent, options.capture, 1, true,
                                  overlay ? &*overlay : nullptr, .04F));
    }
    if (!options.report.empty()) {
        std::ostringstream csv;
        csv.precision(12);
        csv << "frame,tick,mode,frame_ns,gpu_shadow_ns,gpu_pbr_ns,gpu_post_ns,gpu_ui_ns,gpu_ready,"
               "x_m,y_m,z_m,vx_mps,vy_mps,vz_mps,fx_N,fy_N,fz_N,tx_Nm,ty_Nm,tz_Nm,mass_kg,live,"
               "retiring,allocation_bytes,reserved_bytes,flow_phase,drive_power,present_width,"
               "present_height,nav_goal,qx,qy,qz,qw,wx_radps,wy_radps,wz_radps,camera_x_m,"
               "camera_y_m,camera_z_m,camera_qx,camera_qy,camera_qz,camera_qw,camera_fov_deg,"
               "input_focused,capture_requested,capture_accepted,render_x_m,render_y_m,render_z_m,"
               "entity_generation,audio_state,audio_gen,audio_losses,audio_queued_frames,audio_"
               "muted,audio_paused,host_ns\n";
        for (std::size_t i = 0; i < measured; ++i) {
            const auto &s = samples[i];
            csv << i << ',' << s.state.tick.value << ',' << static_cast<unsigned>(s.state.mode)
                << ',' << s.frame_ns << ',' << s.gpu.shadow_ns << ',' << s.gpu.pbr_ns << ','
                << s.gpu.post_ns << ',' << s.gpu.ui_ns << ',' << s.gpu_ready;
            for (const auto v : s.state.body.position_metres)
                csv << ',' << v;
            for (const auto v : s.state.body.velocity_mps)
                csv << ',' << v;
            for (const auto v : s.state.force_newtons)
                csv << ',' << v;
            for (const auto v : s.state.torque_newton_metres)
                csv << ',' << v;
            csv << ',' << s.state.body.mass_kg << ',' << s.memory.live << ',' << s.memory.retiring
                << ',' << s.memory.allocation_bytes << ',' << s.memory.reserved_bytes << ','
                << s.phase << ',' << s.power << ',' << s.extent.width << ',' << s.extent.height
                << ',' << static_cast<unsigned>(s.state.navigation.goal);
            for (const auto v : s.state.body.rotation_xyzw)
                csv << ',' << v;
            for (const auto v : s.state.body.angular_velocity_rps)
                csv << ',' << v;
            for (const auto v : s.camera.camera.position_metres)
                csv << ',' << v;
            for (const auto v : s.camera.camera.rotation_xyzw)
                csv << ',' << v;
            csv << ',' << s.camera.vertical_fov_degrees << ',' << s.focused << ','
                << s.capture_requested << ',' << s.capture_accepted;
            for (const auto v : s.camera.ship.position_metres)
                csv << ',' << v;
            csv << ',' << s.state.entity.generation << ',' << static_cast<unsigned>(s.audio.state)
                << ',' << s.audio.generation << ',' << s.audio.losses << ','
                << s.audio.queued_frames << ',' << s.audio.muted << ',' << s.audio.paused << ','
                << s.host_ns << '\n';
        }
        const auto text = csv.str();
        (void)take(probe::write_exclusive(options.report,
                                          std::as_bytes(std::span(text.data(), text.size()))));
    }
    std::printf("flight_hdr_finite=1 flight_lit_pixels=%zu\n", lit_pixels);
    std::printf("flight_frames=%zu warmup=%u replay=%u walkthrough=%u validation=%u "
                "allocation_probe=%u triangles=%llu contacts=101 "
                "camera_mount=bow_sensor "
                "cpp_new_calls=%llu imgui_calls=%llu gpu_timing_rows=%zu live=%u retiring=%u "
                "allocation_bytes=%llu reserved_bytes=%llu audio_pending=0\n",
                measured, warmup, options.replay ? 1U : 0U, options.walkthrough ? 1U : 0U,
                options.validation ? 1U : 0U, workload::allocation_probe_enabled() ? 1U : 0U,
                static_cast<unsigned long long>(preset.triangles),
                static_cast<unsigned long long>(allocations),
                static_cast<unsigned long long>(ui_allocations),
                static_cast<std::size_t>(std::count_if(samples.begin(), samples.end(),
                                                       [](const auto &s) { return s.gpu_ready; })),
                memory.live, memory.retiring,
                static_cast<unsigned long long>(memory.allocation_bytes),
                static_cast<unsigned long long>(memory.reserved_bytes));
    const auto audio_stats = sound.statistics();
    std::printf(
        "audio_state=%s audio_gen=%llu audio_losses=%llu audio_queue_max=%u audio_muted=%u\n",
        audio_stats.state == audio::State::disconnected ? "offline"
                                                        : (audio_stats.muted ? "muted" : "playing"),
        static_cast<unsigned long long>(audio_stats.generation),
        static_cast<unsigned long long>(audio_stats.losses), audio_stats.maximum_queued_frames,
        audio_stats.muted ? 1U : 0U);
    (void)SDL_SetWindowRelativeMouseMode(context->window(), false);
    (void)sound.close();
    gui.reset();
    require(context->close(), "flight context close failed");
    require(context->validation_errors() == 0, "flight Vulkan validation reported errors");
}
} // namespace ngen::demo
