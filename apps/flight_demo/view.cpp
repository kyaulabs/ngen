#include <Jolt/Jolt.h>

#include "flight_demo/view.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace ngen::demo {
namespace {
bool valid_pose(const flight::Pose &pose) {
    double norm = 0;
    for (const auto value : pose.rotation_xyzw) {
        if (!std::isfinite(value))
            return false;
        norm += static_cast<double>(value) * value;
    }
    return std::abs(norm - 1) < 1e-4 &&
           std::all_of(pose.position_metres.begin(), pose.position_metres.end(),
                       [](double v) { return std::isfinite(v) && std::abs(v) <= 16384; });
}
JPH::Quat quaternion(const flight::Pose &pose) {
    const auto &q = pose.rotation_xyzw;
    return JPH::Quat(q[0], q[1], q[2], q[3]).Normalized();
}
JPH::Mat44 matrix(const render::shader::Matrix &value) {
    const auto &m = value.columns;
    return JPH::Mat44{JPH::Vec4{m[0], m[1], m[2], m[3]}, JPH::Vec4{m[4], m[5], m[6], m[7]},
                      JPH::Vec4{m[8], m[9], m[10], m[11]}, JPH::Vec4{m[12], m[13], m[14], m[15]}};
}
} // namespace
std::variant<FrameView, render::Failure>
present(const Preset &preset, const flight::CameraFrame &camera, VkExtent2D extent) {
    if (extent.width == 0 || extent.height == 0 || extent.width > 8192 || extent.height > 8192 ||
        !valid_pose(camera.ship) || !valid_pose(camera.camera) ||
        !std::isfinite(camera.vertical_fov_degrees) || camera.vertical_fov_degrees < 30 ||
        camera.vertical_fov_degrees > 110 || !std::isfinite(preset.ship_radius) ||
        preset.ship_radius < .1F || preset.ship_radius > 1024)
        return render::Failure{"flight presentation exceeds the local camera profile"};
    FrameView output;
    output.origin_metres = camera.ship.position_metres;
    const auto &origin = output.origin_metres;
    std::array<double, 3> eye{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        eye[axis] = camera.camera.position_metres[axis] - origin[axis];
        output.scene.camera_position[axis] = static_cast<float>(eye[axis]);
    }
    output.scene.camera_position[3] = 1;
    const auto rotation = quaternion(camera.camera);
    const auto right = rotation * JPH::Vec3::sAxisX();
    const auto up = rotation * JPH::Vec3::sAxisY();
    const auto back = rotation * JPH::Vec3::sAxisZ();
    const std::array<std::array<double, 3>, 3> basis{{{right.GetX(), right.GetY(), right.GetZ()},
                                                      {up.GetX(), up.GetY(), up.GetZ()},
                                                      {back.GetX(), back.GetY(), back.GetZ()}}};
    const auto f =
        1 / std::tan(static_cast<double>(camera.vertical_fov_degrees) * std::numbers::pi / 360);
    const auto aspect = static_cast<double>(extent.width) / extent.height;
    constexpr double near_plane = .1, far_plane = 8192;
    constexpr auto z_scale = far_plane / (near_plane - far_plane);
    constexpr auto z_offset = far_plane * near_plane / (near_plane - far_plane);
    auto &projection = output.scene.view_projection.columns;
    double x_eye = 0, y_eye = 0, z_eye = 0;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        projection[axis * 4] = static_cast<float>(f / aspect * basis[0][axis]);
        projection[axis * 4 + 1] = static_cast<float>(-f * basis[1][axis]);
        projection[axis * 4 + 2] = static_cast<float>(z_scale * basis[2][axis]);
        projection[axis * 4 + 3] = static_cast<float>(-basis[2][axis]);
        x_eye += basis[0][axis] * eye[axis];
        y_eye += basis[1][axis] * eye[axis];
        z_eye += basis[2][axis] * eye[axis];
    }
    projection[12] = static_cast<float>(-f / aspect * x_eye);
    projection[13] = static_cast<float>(f * y_eye);
    projection[14] = static_cast<float>(-z_scale * z_eye + z_offset);
    projection[15] = static_cast<float>(z_eye);
    const auto model =
        JPH::Mat44::sRotation(quaternion(camera.ship)) * matrix(preset.ship.asset_to_body);
    for (JPH::uint column = 0; column < 4; ++column) {
        const auto value = model.GetColumn4(column);
        const auto offset = std::size_t{column} * 4;
        auto &m = output.scene.root_transform.columns;
        m[offset] = value.GetX();
        m[offset + 1] = value.GetY();
        m[offset + 2] = value.GetZ();
        m[offset + 3] = value.GetW();
    }
    constexpr std::array<std::array<float, 3>, 3> sun{{{.70710678F, 0, -.70710678F},
                                                       {-.40824829F, .81649658F, -.40824829F},
                                                       {.57735027F, .57735027F, .57735027F}}};
    auto &light = output.scene.light_view_projection.columns;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        light[axis * 4] = sun[0][axis] / (preset.ship_radius * 1.35F);
        light[axis * 4 + 1] = -sun[1][axis] / (preset.ship_radius * 1.35F);
        light[axis * 4 + 2] = -sun[2][axis] / (preset.ship_radius * 8);
    }
    light[14] = .5F;
    light[15] = 1;
    output.scene.key_direction_intensity = {-1, -1, -1, 1.5F};
    output.asteroids = preset.field.instances;
    for (auto &instance : output.asteroids)
        for (std::size_t axis = 0; axis < 3; ++axis)
            instance.model.columns[12 + axis] = static_cast<float>(
                static_cast<double>(instance.model.columns[12 + axis]) - origin[axis]);
    return output;
}
std::optional<std::array<float, 2>> project_target(const FrameView &view,
                                                   const std::array<double, 3> &point,
                                                   std::array<float, 2> size) {
    if (!std::all_of(size.begin(), size.end(), [](float value) {
            return std::isfinite(value) && value > 0 && value <= 8192;
        }))
        return std::nullopt;
    std::array<double, 4> local{point[0] - view.origin_metres[0], point[1] - view.origin_metres[1],
                                point[2] - view.origin_metres[2], 1},
        clip{};
    for (std::size_t row = 0; row < 4; ++row)
        for (std::size_t column = 0; column < 4; ++column)
            clip[row] += view.scene.view_projection.columns[column * 4 + row] * local[column];
    if (std::all_of(clip.begin(), clip.end(), [](double value) { return std::isfinite(value); }) &&
        clip[3] > 0 && clip[2] >= 0 && clip[2] <= clip[3] && std::abs(clip[0]) <= clip[3] &&
        std::abs(clip[1]) <= clip[3])
        return std::array<float, 2>{static_cast<float>(clip[0] / clip[3] * .5 + .5) * size[0],
                                    static_cast<float>(clip[1] / clip[3] * .5 + .5) * size[1]};
    return std::nullopt;
}
} // namespace ngen::demo
