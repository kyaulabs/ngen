#include <Jolt/Jolt.h>

#include "flight/camera.hpp"
#include <Jolt/Math/DMat44.h>
#include <algorithm>
#include <cmath>
#include <numbers>

namespace ngen::flight {
namespace {
bool valid_transform(const std::array<double, 3> &position, const std::array<float, 4> &rotation,
                     double maximum_position = 16384) {
    double length = 0;
    for (const auto value : rotation) {
        if (!std::isfinite(value))
            return false;
        length += static_cast<double>(value) * value;
    }
    return std::abs(length - 1) < 1e-4 &&
           std::all_of(position.begin(), position.end(), [maximum_position](double value) {
               return std::isfinite(value) && std::abs(value) <= maximum_position;
           });
}
JPH::Quat quaternion(const std::array<float, 4> &values) {
    return JPH::Quat(values[0], values[1], values[2], values[3]).Normalized();
}
std::array<float, 4> components(JPH::QuatArg value) {
    return {value.GetX(), value.GetY(), value.GetZ(), value.GetW()};
}
bool valid_sphere(const CameraSphere &sphere) {
    return std::isfinite(sphere.radius_metres) && sphere.radius_metres >= .1F &&
           sphere.radius_metres <= 1024 &&
           std::all_of(sphere.centre_metres.begin(), sphere.centre_metres.end(),
                       [](double v) { return std::isfinite(v) && std::abs(v) <= 16384; });
}
bool valid_motion(CameraMotion motion) {
    return std::isfinite(motion.yaw_radians) && std::isfinite(motion.pitch_radians) &&
           std::isfinite(motion.zoom_steps) &&
           std::abs(motion.yaw_radians) <= std::numbers::pi_v<float> &&
           std::abs(motion.pitch_radians) <= std::numbers::pi_v<float> &&
           std::abs(motion.zoom_steps) <= 16;
}
} // namespace
CameraRig::CameraRig(float radius)
    : radius_(radius), distance_(std::max(3 * radius_, radius_ + .5F)) {}
Result<CameraRig> CameraRig::create(float followed_radius_metres) {
    if (!std::isfinite(followed_radius_metres) || followed_radius_metres < .1F ||
        followed_radius_metres > 1024)
        return Error::invalid;
    return CameraRig(followed_radius_metres);
}
Result<std::monostate> CameraRig::orbit(CameraMotion motion) {
    if (!valid_motion(motion))
        return Error::invalid;
    yaw_ = std::remainder(yaw_ + motion.yaw_radians, 2 * std::numbers::pi_v<float>);
    pitch_ = std::clamp(pitch_ + motion.pitch_radians, -1.35F, 1.35F);
    const auto radius = focus_ ? focus_->radius_metres : radius_;
    distance_ = std::clamp(distance_ * std::exp(-.15F * motion.zoom_steps), radius + .5F, 4096.0F);
    return std::monostate{};
}
Result<std::monostate> CameraRig::focus(std::optional<CameraSphere> target) {
    if (target && !valid_sphere(*target))
        return Error::invalid;
    focus_ = target;
    const auto radius = focus_ ? focus_->radius_metres : radius_;
    distance_ = std::max(3 * radius, radius + .5F);
    return std::monostate{};
}
Result<std::monostate> CameraRig::look(CameraMotion motion) {
    if (!valid_motion(motion) || motion.zoom_steps != 0)
        return Error::invalid;
    look_yaw_ = std::clamp(look_yaw_ + motion.yaw_radians, -1.2F, 1.2F);
    look_pitch_ = std::clamp(look_pitch_ + motion.pitch_radians, -1.0F, 1.0F);
    return std::monostate{};
}
void CameraRig::center_look() noexcept {
    look_yaw_ = 0;
    look_pitch_ = 0;
}
Result<CameraFrame> CameraRig::sample(FrameSamples samples, float fraction,
                                      std::span<const CameraSphere> obstacles) const {
    if (!std::isfinite(fraction) || fraction < 0 || fraction > 1 ||
        obstacles.size() > physics::World::body_capacity ||
        !std::all_of(obstacles.begin(), obstacles.end(), valid_sphere))
        return Error::invalid;
    const auto &state = samples.current;
    if (!valid_transform(state.body.position_metres, state.body.rotation_xyzw) ||
        !valid_transform(state.flight_camera.position_metres, state.flight_camera.rotation_xyzw,
                         1024) ||
        !std::isfinite(state.flight_camera.vertical_fov_degrees) ||
        state.flight_camera.vertical_fov_degrees < 30 ||
        state.flight_camera.vertical_fov_degrees > 110 ||
        (state.mode != Mode::direct && state.mode != Mode::tactical))
        return Error::invalid;
    CameraFrame frame;
    frame.ship = {state.body.position_metres, components(quaternion(state.body.rotation_xyzw))};
    const auto &previous = samples.previous;
    if (previous.entity == state.entity && state.tick.value > previous.tick.value &&
        state.tick.value - previous.tick.value == 1) {
        if (!valid_transform(previous.body.position_metres, previous.body.rotation_xyzw))
            return Error::invalid;
        for (std::size_t i = 0; i < frame.ship.position_metres.size(); ++i)
            frame.ship.position_metres[i] = std::lerp(previous.body.position_metres[i],
                                                      state.body.position_metres[i], fraction);
        frame.ship.rotation_xyzw =
            components(quaternion(previous.body.rotation_xyzw)
                           .SLERP(quaternion(state.body.rotation_xyzw), fraction)
                           .Normalized());
    }
    frame.camera = frame.ship;
    frame.vertical_fov_degrees = state.flight_camera.vertical_fov_degrees;
    if (state.mode == Mode::tactical) {
        const auto &anchor = focus_ ? focus_->centre_metres : frame.ship.position_metres;
        const JPH::DVec3 origin{anchor[0], anchor[1], anchor[2]};
        const CameraSphere followed{frame.ship.position_metres, radius_};
        bool found = false;
        // At most eight booms over the bounded contact-domain proxy set.
        for (const auto offset :
             {0.0F, .4F, -.4F, .8F, -.8F, 1.6F, -1.6F, std::numbers::pi_v<float>}) {
            const auto yaw = yaw_ + offset;
            const auto direction = JPH::DVec3{std::sin(yaw) * std::cos(pitch_), std::sin(pitch_),
                                              std::cos(yaw) * std::cos(pitch_)}
                                       .Normalized();
            double distance = distance_;
            bool blocked = false;
            for (std::size_t i = 0; i <= obstacles.size(); ++i) {
                if (i == 0 && !focus_)
                    continue;
                const auto &obstacle = i == 0 ? followed : obstacles[i - 1];
                if (focus_ && obstacle.centre_metres == focus_->centre_metres &&
                    obstacle.radius_metres == focus_->radius_metres)
                    continue;
                const auto &p = obstacle.centre_metres;
                const auto delta = origin - JPH::DVec3{p[0], p[1], p[2]};
                const auto radius = static_cast<double>(obstacle.radius_metres) + .25;
                const auto c = delta.LengthSq() - radius * radius;
                if (c <= 0) {
                    blocked = true;
                    break;
                }
                const auto b = delta.Dot(direction);
                const auto discriminant = b * b - c;
                if (discriminant >= 0) {
                    const auto entry = -b - std::sqrt(discriminant);
                    if (entry >= 0)
                        distance = std::min(distance, entry);
                }
            }
            if (blocked || distance < (focus_ ? focus_->radius_metres : radius_) + .5)
                continue;
            const auto eye = origin + direction * distance;
            frame.camera.position_metres = {eye.GetX(), eye.GetY(), eye.GetZ()};
            frame.camera.rotation_xyzw =
                components((JPH::Quat::sRotation(JPH::Vec3::sAxisY(), yaw) *
                            JPH::Quat::sRotation(JPH::Vec3::sAxisX(), -pitch_))
                               .Normalized());
            found = true;
            break;
        }
        if (!found)
            return Error::busy;
        frame.vertical_fov_degrees = 70;
    } else {
        const auto rotation = quaternion(frame.ship.rotation_xyzw);
        const auto &p = frame.ship.position_metres;
        const auto transform = JPH::DMat44::sRotationTranslation(rotation, {p[0], p[1], p[2]});
        const auto &mount = state.flight_camera.position_metres;
        const auto eye = transform * JPH::DVec3{mount[0], mount[1], mount[2]};
        frame.camera.position_metres = {eye.GetX(), eye.GetY(), eye.GetZ()};
        const auto look_rotation = JPH::Quat::sRotation(JPH::Vec3::sAxisY(), look_yaw_) *
                                   JPH::Quat::sRotation(JPH::Vec3::sAxisX(), -look_pitch_);
        frame.camera.rotation_xyzw =
            components((rotation * quaternion(state.flight_camera.rotation_xyzw) * look_rotation)
                           .Normalized());
    }
    return frame;
}
} // namespace ngen::flight
