#include "scene_workload/fixture.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <random>

namespace ngen::workload {
namespace {
using Vec = std::array<float, 3>;
Vec subtract(Vec a, const Vec &b) {
    for (std::size_t i = 0; i < 3; ++i)
        a[i] -= b[i];
    return a;
}
Vec cross(const Vec &a, const Vec &b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
Vec normalized(Vec a) {
    const auto length = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    for (auto &value : a)
        value /= length;
    return a;
}
Vec middle(const Vec &a, const Vec &b) {
    return normalized({a[0] + b[0], a[1] + b[1], a[2] + b[2]});
}
Vec deformed(Vec position) {
    const auto scale = 1 + .12F * position[0] * position[1] + .08F * position[1] * position[2] -
                       .06F * position[2] * position[0];
    for (auto &value : position)
        value *= scale;
    return position;
}
constexpr render::shader::Matrix identity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
} // namespace
std::variant<Fixture, render::Failure> make_fixture(const assets::Bounds &bounds,
                                                    std::uint32_t seed) {
    Fixture output;
    output.ship_bounds = bounds;
    output.seed = seed;
    double radius_squared = 0;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const double low = bounds.minimum[axis], high = bounds.maximum[axis];
        if (!std::isfinite(low) || !std::isfinite(high) || low > high)
            return render::Failure{"flight fixture requires finite ordered ship bounds"};
        const auto center = (low + high) * .5;
        const auto radius = (high - low) * .5;
        if (std::abs(center) > 100000 || radius > 10000)
            return render::Failure{"flight fixture exceeds the local-space profile"};
        output.center[axis] = static_cast<float>(center);
        radius_squared += radius * radius;
    }
    output.radius = static_cast<float>(std::max(1.0, std::sqrt(radius_squared)));
    if (output.radius > 10000)
        return render::Failure{"flight fixture exceeds the local-space profile"};
    constexpr float phi = 1.61803398875F;
    std::array<Vec, 12> points{{{-1, phi, 0},
                                {1, phi, 0},
                                {-1, -phi, 0},
                                {1, -phi, 0},
                                {0, -1, phi},
                                {0, 1, phi},
                                {0, -1, -phi},
                                {0, 1, -phi},
                                {phi, 0, -1},
                                {phi, 0, 1},
                                {-phi, 0, -1},
                                {-phi, 0, 1}}};
    for (auto &point : points)
        point = normalized(point);
    constexpr std::array<std::array<std::size_t, 3>, 20> faces{
        {{0, 11, 5},  {0, 5, 1},  {0, 1, 7},  {0, 7, 10}, {0, 10, 11}, {1, 5, 9}, {5, 11, 4},
         {11, 10, 2}, {10, 7, 6}, {7, 1, 8},  {3, 9, 4},  {3, 4, 2},   {3, 2, 6}, {3, 6, 8},
         {3, 8, 9},   {4, 9, 5},  {2, 4, 11}, {6, 2, 10}, {8, 6, 7},   {9, 8, 1}}};
    std::size_t next = 0;
    const auto triangle = [&](Vec a, Vec b, Vec c) {
        a = deformed(a);
        b = deformed(b);
        c = deformed(c);
        auto normal = normalized(cross(subtract(b, a), subtract(c, a)));
        if (normal[0] * a[0] + normal[1] * a[1] + normal[2] * a[2] < 0) {
            std::swap(b, c);
            for (auto &value : normal)
                value = -value;
        }
        const auto tangent = normalized(subtract(b, a));
        const std::array vertices{a, b, c};
        for (std::size_t corner = 0; corner < 3; ++corner) {
            auto &vertex = output.vertices[next];
            vertex.position = vertices[corner];
            vertex.normal = normal;
            vertex.tangent = {tangent[0], tangent[1], tangent[2], 1};
            vertex.uv = {corner == 1 ? 1.0F : 0.0F, corner == 2 ? 1.0F : 0.0F};
            vertex.uv1 = vertex.uv;
            output.indices[next] = static_cast<std::uint32_t>(next);
            ++next;
        }
    };
    for (const auto &face : faces) {
        const auto a = points[face[0]], b = points[face[1]], c = points[face[2]];
        const auto ab = middle(a, b), bc = middle(b, c), ca = middle(c, a);
        triangle(a, ab, ca);
        triangle(b, bc, ab);
        triangle(c, ca, bc);
        triangle(ab, bc, ca);
    }
    std::mt19937 random(seed);
    const auto unit = [&] {
        return static_cast<float>(static_cast<std::uint32_t>(random()) >> 8U) / 16777216.0F;
    };
    for (std::size_t i = 0; i < output.instances.size(); ++i) {
        const auto azimuth = 2.39996323F * static_cast<float>(i) + unit() * .1F;
        const auto y = 1 - 2 * (static_cast<float>(i) + .5F) / 100;
        const auto radial = std::sqrt(std::max(0.0F, 1 - y * y));
        const auto distance = output.radius * (1.08F + .06F * unit());
        const Vec position{radial * std::cos(azimuth), y, radial * std::sin(azimuth)};
        const auto yaw = unit() * 2 * std::numbers::pi_v<float>;
        const auto pitch = unit() * 2 * std::numbers::pi_v<float>;
        const auto ca = std::cos(yaw), sa = std::sin(yaw), cb = std::cos(pitch),
                   sb = std::sin(pitch);
        const std::array<Vec, 3> columns{
            {{ca * cb, sa * cb, -sb}, {-sa, ca, 0}, {ca * sb, sa * sb, cb}}};
        const auto size = output.radius * (.012F + .012F * unit());
        auto &instance = output.instances[i];
        instance.model = instance.normalMatrix = identity;
        for (std::size_t column = 0; column < 3; ++column) {
            const auto scale = size * (.8F + .4F * unit());
            for (std::size_t row = 0; row < 3; ++row) {
                instance.model.columns[column * 4 + row] = columns[column][row] * scale;
                instance.normalMatrix.columns[column * 4 + row] = columns[column][row] / scale;
            }
            instance.model.columns[12 + column] =
                output.center[column] + position[column] * distance;
        }
    }
    return output;
}
std::variant<render::SceneView, render::Failure> view(const Fixture &fixture, std::uint32_t frame,
                                                      VkExtent2D extent) {
    if (extent.width == 0 || extent.height == 0 || extent.width > 8192 || extent.height > 8192)
        return render::Failure{"flight view extent is outside 1..8192"};
    if (!std::isfinite(fixture.radius) || fixture.radius < 1 || fixture.radius > 10000 ||
        !std::all_of(fixture.center.begin(), fixture.center.end(),
                     [](float value) { return std::isfinite(value) && std::abs(value) <= 100000; }))
        return render::Failure{"flight view requires admitted local bounds"};
    const auto angle = static_cast<float>(frame % replay_frames) * 2 * std::numbers::pi_v<float> /
                       static_cast<float>(replay_frames);
    constexpr float pitch = .42F;
    const Vec right{std::cos(angle), 0, -std::sin(angle)};
    const Vec back{std::sin(angle) * std::cos(pitch), std::sin(pitch),
                   std::cos(angle) * std::cos(pitch)};
    const auto up = cross(back, right);
    const auto aspect = static_cast<float>(extent.width) / static_cast<float>(extent.height);
    const auto half_height = fixture.radius * 1.35F * std::max(1.0F, 1 / aspect);
    const auto projection = [&](const std::array<Vec, 3> &basis, std::array<float, 2> half_extent) {
        const auto &[r, u, b] = basis;
        const auto [half_width, height] = half_extent;
        render::shader::Matrix result{};
        const auto depth = fixture.radius * 8;
        float r_center = 0, u_center = 0, b_center = 0;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            result.columns[axis * 4] = r[axis] / half_width;
            result.columns[axis * 4 + 1] = -u[axis] / height;
            result.columns[axis * 4 + 2] = -b[axis] / depth;
            r_center += r[axis] * fixture.center[axis];
            u_center += u[axis] * fixture.center[axis];
            b_center += b[axis] * fixture.center[axis];
        }
        result.columns[12] = -r_center / half_width;
        result.columns[13] = u_center / height;
        result.columns[14] = .5F + b_center / depth;
        result.columns[15] = 1;
        return result;
    };
    render::SceneView output{};
    output.view_projection = projection({right, up, back}, {half_height * aspect, half_height});
    const Vec light_right{.70710678F, 0, -.70710678F};
    const Vec light_up{-.40824829F, .81649658F, -.40824829F};
    const Vec light_back{.57735027F, .57735027F, .57735027F};
    output.light_view_projection = projection({light_right, light_up, light_back},
                                              {fixture.radius * 1.35F, fixture.radius * 1.35F});
    for (std::size_t axis = 0; axis < 3; ++axis)
        output.camera_position[axis] = fixture.center[axis] + back[axis] * fixture.radius * 4;
    output.key_direction_intensity = {-1, -1, -1, 1.5F};
    return output;
}
} // namespace ngen::workload
