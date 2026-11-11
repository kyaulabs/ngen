#pragma once
#include "flight/camera.hpp"

namespace ngen::flight {
enum class FlightKey : std::uint8_t {
    forward,
    backward,
    left,
    right,
    up,
    down,
    pitch_up,
    pitch_down,
    yaw_left,
    yaw_right,
    roll_left,
    roll_right,
    brake,
    free_look,
    count
};
struct PointerDelta {
    float x_pixels{}, y_pixels{};
};
struct CameraEvents {
    CameraMotion orbit, look;
    bool center_look{};
};
// Platform-neutral held input. SDL owns event collection and relative-pointer
// application; this value owns no window and can only emit intent/deltas.
// Captured direct motion is a virtual stick held until release/recentering,
// not a rate multiplied by render cadence. Apply camera centering before deltas.
class FlightInput {
  public:
    [[nodiscard]] Result<std::monostate> key(FlightKey key, bool pressed);
    [[nodiscard]] Result<std::monostate> mode(Mode mode);
    void focus(bool focused) noexcept;
    // Escape/UI handoff: neutralize keys, pointer and pending camera input.
    void release() noexcept;
    void pointer(bool pressed) noexcept;
    [[nodiscard]] Result<std::monostate> motion(PointerDelta delta);
    [[nodiscard]] Result<std::monostate> wheel(float steps);
    [[nodiscard]] Pilot pilot() const;
    [[nodiscard]] CameraEvents take_camera() noexcept;
    [[nodiscard]] bool capture() const noexcept;

  private:
    std::array<bool, static_cast<std::size_t>(FlightKey::count)> held_{};
    Mode mode_{Mode::tactical};
    bool focused_{true}, pointer_{};
    float stick_x_{}, stick_y_{};
    CameraEvents camera_;
};
} // namespace ngen::flight
