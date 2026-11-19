#pragma once
#include "flight/flight.hpp"
#include <optional>
#include <span>

namespace ngen::flight {
struct Pose {
    std::array<double, 3> position_metres{};
    std::array<float, 4> rotation_xyzw{0, 0, 0, 1};
};
struct CameraFrame {
    Pose ship, camera;
    float vertical_fov_degrees{70};
};
// Borrowed only during sample; no simulation reference or mutable owner is kept.
struct FrameSamples {
    const Snapshot &previous;
    const Snapshot &current;
    // An optional stationary presentation basis from this domain. Focus and
    // obstacle positions are expressed in the selected output basis.
    const coordinates::Frame *basis{};
};
struct CameraSphere {
    std::array<double, 3> centre_metres{};
    float radius_metres{};
};
// Event deltas, not per-render-frame rates or simulation input.
struct CameraMotion {
    float yaw_radians{}, pitch_radians{}, zoom_steps{};
};
class CameraRig {
  public:
    [[nodiscard]] static Result<CameraRig> create(float followed_radius_metres);
    [[nodiscard]] Result<std::monostate> orbit(CameraMotion motion);
    [[nodiscard]] Result<std::monostate> focus(std::optional<CameraSphere> target);
    [[nodiscard]] Result<std::monostate> look(CameraMotion motion);
    // Input adapters call this on free-look release/focus loss; no ship is steered.
    void center_look() noexcept;
    // Interpolate consecutive snapshots in the current origin; resets/gaps
    // snap and an unresolved epoch change is stale. Borrow at most
    // World::body_capacity external sphere proxies for this call. A focused
    // tactical view also protects the controlled hull. Never displace an authored
    // first-person eye; return busy if eight bounded tactical booms are unsafe.
    [[nodiscard]] Result<CameraFrame> sample(FrameSamples samples, float fraction,
                                             std::span<const CameraSphere> obstacles = {}) const;

  private:
    explicit CameraRig(float radius);
    float radius_, distance_;
    float yaw_{.65F}, pitch_{.35F};
    float look_yaw_{}, look_pitch_{};
    std::optional<CameraSphere> focus_;
};
} // namespace ngen::flight
