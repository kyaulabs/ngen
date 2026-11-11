#include "flight/input.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace ngen::flight {
Result<std::monostate> FlightInput::key(FlightKey key, bool pressed) {
    const auto index = static_cast<std::size_t>(key);
    if (index >= held_.size())
        return Error::invalid;
    if (focused_) {
        if (key == FlightKey::free_look && held_[index] != pressed) {
            stick_x_ = stick_y_ = 0;
            camera_.look = {};
            if (!pressed)
                camera_.center_look = true;
        }
        held_[index] = pressed;
    }
    return std::monostate{};
}
Result<std::monostate> FlightInput::mode(Mode mode) {
    if (mode != Mode::tactical && mode != Mode::direct)
        return Error::invalid;
    if (mode_ != mode)
        release();
    mode_ = mode;
    return std::monostate{};
}
void FlightInput::focus(bool focused) noexcept {
    focused_ = focused;
    if (!focused)
        release();
}
void FlightInput::release() noexcept {
    held_.fill(false);
    pointer_ = false;
    stick_x_ = stick_y_ = 0;
    camera_ = {};
    camera_.center_look = true;
}
void FlightInput::pointer(bool pressed) noexcept {
    pointer_ = pressed && focused_;
    if (!pointer_) {
        stick_x_ = stick_y_ = 0;
        camera_.look = {};
        camera_.center_look = true;
    }
}
Result<std::monostate> FlightInput::motion(PointerDelta delta) {
    if (!std::isfinite(delta.x_pixels) || !std::isfinite(delta.y_pixels) ||
        std::abs(delta.x_pixels) > 32768 || std::abs(delta.y_pixels) > 32768)
        return Error::invalid;
    if (capture() &&
        (mode_ == Mode::tactical || held_[static_cast<std::size_t>(FlightKey::free_look)])) {
        constexpr auto limit = std::numbers::pi_v<float>;
        auto &camera = mode_ == Mode::tactical ? camera_.orbit : camera_.look;
        camera.yaw_radians = std::clamp(camera.yaw_radians - delta.x_pixels * .003F, -limit, limit);
        camera.pitch_radians =
            std::clamp(camera.pitch_radians + delta.y_pixels * .003F, -limit, limit);
    } else if (capture() && mode_ == Mode::direct) {
        stick_x_ = std::clamp(stick_x_ + delta.x_pixels * .004F, -1.0F, 1.0F);
        stick_y_ = std::clamp(stick_y_ + delta.y_pixels * .004F, -1.0F, 1.0F);
    }
    return std::monostate{};
}
Result<std::monostate> FlightInput::wheel(float steps) {
    if (!std::isfinite(steps) || std::abs(steps) > 16)
        return Error::invalid;
    if (focused_ && mode_ == Mode::tactical)
        camera_.orbit.zoom_steps = std::clamp(camera_.orbit.zoom_steps + steps, -16.0F, 16.0F);
    return std::monostate{};
}
Pilot FlightInput::pilot() const {
    Pilot result;
    if (!focused_)
        return result;
    const auto held = [this](FlightKey key) {
        return static_cast<float>(held_[static_cast<std::size_t>(key)]);
    };
    result.brake = held(FlightKey::brake) != 0;
    if (mode_ != Mode::direct)
        return result;
    result.thrust = {held(FlightKey::right) - held(FlightKey::left),
                     held(FlightKey::up) - held(FlightKey::down),
                     held(FlightKey::backward) - held(FlightKey::forward)};
    result.rotation = {
        std::clamp(held(FlightKey::pitch_up) - held(FlightKey::pitch_down) - stick_y_, -1.0F, 1.0F),
        std::clamp(held(FlightKey::yaw_left) - held(FlightKey::yaw_right) - stick_x_, -1.0F, 1.0F),
        held(FlightKey::roll_left) - held(FlightKey::roll_right)};
    return result;
}
CameraEvents FlightInput::take_camera() noexcept {
    const auto events = camera_;
    camera_ = {};
    return events;
}
bool FlightInput::capture() const noexcept { return focused_ && pointer_; }
} // namespace ngen::flight
