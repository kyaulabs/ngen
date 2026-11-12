#include "flight_demo/preset.hpp"
#include "flight_demo/frigate_rcs.hpp"
#include <algorithm>
#include <cmath>
#include <limits>

#include <Jolt/Jolt.h>

#include <Jolt/Math/Quat.h>

namespace ngen::demo {
namespace {
using Point = std::array<double, 3>;
bool proper_rigid(const render::shader::Matrix &matrix) {
    const auto &m = matrix.columns;
    if (!std::all_of(m.begin(), m.end(), [](float v) { return std::isfinite(v); }) || m[3] != 0 ||
        m[7] != 0 || m[11] != 0 || m[15] != 1 || std::abs(m[12]) > 16384 ||
        std::abs(m[13]) > 16384 || std::abs(m[14]) > 16384)
        return false;
    for (std::size_t a = 0; a < 3; ++a)
        for (std::size_t b = a; b < 3; ++b) {
            double dot = 0;
            for (std::size_t axis = 0; axis < 3; ++axis)
                dot += static_cast<double>(m[a * 4 + axis]) * m[b * 4 + axis];
            if (std::abs(dot - (a == b ? 1 : 0)) > 1e-4)
                return false;
        }
    const Point a{m[0], m[1], m[2]}, b{m[4], m[5], m[6]}, c{m[8], m[9], m[10]};
    const auto determinant = a[0] * (b[1] * c[2] - b[2] * c[1]) -
                             b[0] * (a[1] * c[2] - a[2] * c[1]) +
                             c[0] * (a[1] * b[2] - a[2] * b[1]);
    return std::abs(determinant - 1) <= 1e-4;
}
Point transformed(const std::array<float, 16> &matrix, const Point &point) {
    Point output{};
    for (std::size_t row = 0; row < 3; ++row) {
        output[row] = matrix[12 + row];
        for (std::size_t column = 0; column < 3; ++column)
            output[row] += static_cast<double>(matrix[column * 4 + row]) * point[column];
    }
    return output;
}
struct BoxBounds {
    Point minimum{std::numeric_limits<double>::max(), std::numeric_limits<double>::max(),
                  std::numeric_limits<double>::max()};
    Point maximum{-std::numeric_limits<double>::max(), -std::numeric_limits<double>::max(),
                  -std::numeric_limits<double>::max()};
    void include(const Point &point) {
        for (std::size_t axis = 0; axis < 3; ++axis) {
            minimum[axis] = std::min(minimum[axis], point[axis]);
            maximum[axis] = std::max(maximum[axis], point[axis]);
        }
    }
};
Point corner(const assets::Bounds &bounds, unsigned index) {
    Point point{};
    for (std::size_t axis = 0; axis < 3; ++axis)
        point[axis] = (index & (1U << axis)) ? bounds.maximum[axis] : bounds.minimum[axis];
    return point;
}
} // namespace
ShipProfile frigate_profile() {
    ShipProfile profile;
    profile.flight.hull = {{36, 23, 162}, {}, 12000000, true};
    profile.flight.maximum_force_newtons = {24000000.0F, 24000000.0F, 72000000.0F};
    profile.flight.maximum_torque_newton_metres = {1.72e11F, 1.76e11F, 1.17e10F};
    profile.flight.maximum_speed_mps = 100;
    profile.flight.maximum_angular_speed_rps = .37F;
    // Authored bow navigation sensor, inside the conservative contact proxy but
    // ahead of opaque geometry. This is ship data, not a universal camera offset.
    profile.flight.flight_camera = {{0, 6, -161}, {0, 0, 0, 1}, 75};
    profile.asset_to_body.columns = {0, 0, 1, 0, 0, 1, 0, 0, -1, 0, 0, 0, 0, -2.07433414F, 0, 1};
    profile.expected_package_crc = 0x423256f5U;
    profile.rcs_mix = frigate_rcs_mix;
    return profile;
}
std::variant<RcsLevels, render::Failure> rcs_levels(const ShipProfile &profile,
                                                    const std::array<float, 4> &q,
                                                    const std::array<float, 3> &world_force,
                                                    const std::array<float, 3> &world_torque) {
    double norm = 0;
    for (const auto value : q) {
        if (!std::isfinite(value))
            return render::Failure{"RCS interval attitude must be finite and normalized"};
        norm += static_cast<double>(value) * value;
    }
    if (std::abs(norm - 1) > 1e-4)
        return render::Failure{"RCS interval attitude must be finite and normalized"};
    for (std::size_t axis = 0; axis < 3; ++axis)
        if (!std::isfinite(world_force[axis]) || !std::isfinite(world_torque[axis]) ||
            !std::isfinite(profile.flight.maximum_force_newtons[axis]) ||
            !std::isfinite(profile.flight.maximum_torque_newton_metres[axis]) ||
            profile.flight.maximum_force_newtons[axis] <= 0 ||
            profile.flight.maximum_torque_newton_metres[axis] <= 0)
            return render::Failure{"RCS wrench and positive actuator limits must be finite"};
    for (const auto &bank : profile.rcs_mix)
        for (const auto value : bank)
            if (!std::isfinite(value) || value < 0 || value > 1)
                return render::Failure{"RCS visual calibration must be finite unit weights"};
    const auto inverse = JPH::Quat(q[0], q[1], q[2], q[3]).Normalized().Conjugated();
    const auto force = inverse * JPH::Vec3{world_force[0], world_force[1], world_force[2]};
    const auto torque = inverse * JPH::Vec3{world_torque[0], world_torque[1], world_torque[2]};
    const std::array<float, 6> local{force.GetX(),  force.GetY(),  force.GetZ(),
                                     torque.GetX(), torque.GetY(), torque.GetZ()};
    RcsLevels output{};
    for (std::size_t axis = 0; axis < 6; ++axis) {
        if (!std::isfinite(local[axis]))
            return render::Failure{"RCS local wrench exceeds finite storage"};
        if (axis == 2 && local[axis] < 0)
            continue; // The accepted rear drive already represents forward thrust.
        const auto limit = axis < 3 ? profile.flight.maximum_force_newtons[axis]
                                    : profile.flight.maximum_torque_newton_metres[axis - 3];
        const auto power = std::clamp(local[axis] / limit, -1.0F, 1.0F);
        const auto column = axis * 2 + (power < 0 ? 1U : 0U);
        for (std::size_t bank = 0; bank < rcs_bank_count; ++bank)
            output[bank] += profile.rcs_mix[bank][column] * std::abs(power);
    }
    const auto largest = std::max(1.0F, *std::max_element(output.begin(), output.end()));
    for (auto &value : output)
        value /= largest;
    return output;
}
std::variant<Preset, render::Failure> make_preset(const assets::PackagePin &package,
                                                  const ShipProfile &profile) {
    if (!package || !proper_rigid(profile.asset_to_body) ||
        (profile.expected_package_crc != 0 && package->checksum != profile.expected_package_crc))
        return render::Failure{"ship profile does not match its immutable package"};
    Preset output;
    output.ship = profile;
    BoxBounds hull, render_bounds;
    std::size_t opaque_instances = 0;
    for (const auto &instance : package->instances) {
        const auto &primitive = package->primitives[instance.primitive];
        output.triangles +=
            primitive.lods[std::min(std::size_t{2}, primitive.lods.size() - 1)].index_count / 3;
        if (primitive.material != UINT32_MAX && package->materials[primitive.material].alpha == 2)
            continue;
        ++opaque_instances;
        for (unsigned i = 0; i < 8; ++i)
            hull.include(transformed(profile.asset_to_body.columns,
                                     transformed(instance.transform, corner(primitive.bounds, i))));
    }
    if (opaque_instances == 0 || output.triangles + 8000 > 250000 ||
        package->instances.size() >= 256)
        return render::Failure{"flight preset exceeds the visible hull/draw profile"};
    output.triangles += 8000;
    for (unsigned i = 0; i < 8; ++i)
        render_bounds.include(
            transformed(profile.asset_to_body.columns, corner(package->bounds, i)));
    assets::Bounds world_bounds;
    double radius_squared = 0;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        const auto half = profile.flight.hull.half_extent_metres[axis];
        if (!std::isfinite(half) || half < .1F || half > 256 ||
            !std::isfinite(hull.minimum[axis]) || !std::isfinite(hull.maximum[axis]) ||
            hull.minimum[axis] < -half - .001 || hull.maximum[axis] > half + .001)
            return render::Failure{"authored contact hull does not enclose opaque ship geometry"};
        radius_squared += static_cast<double>(half) * half;
        const auto origin = profile.flight.hull.position_metres[axis];
        const auto low = std::min(render_bounds.minimum[axis], -static_cast<double>(half)) + origin;
        const auto high = std::max(render_bounds.maximum[axis], static_cast<double>(half)) + origin;
        if (!std::isfinite(low) || !std::isfinite(high) || std::abs(low) > 4096 ||
            std::abs(high) > 4096)
            return render::Failure{"flight preset lies outside the local scene envelope"};
        world_bounds.minimum[axis] = static_cast<float>(low);
        world_bounds.maximum[axis] = static_cast<float>(high);
    }
    output.ship_radius = static_cast<float>(std::sqrt(radius_squared));
    auto field = workload::make_fixture(world_bounds);
    if (const auto *failure = std::get_if<render::Failure>(&field))
        return *failure;
    output.field = std::get<workload::Fixture>(std::move(field));
    for (std::size_t i = 0; i < output.field.instances.size(); ++i) {
        auto &instance = output.field.instances[i];
        if (i < 50) {
            // Retain the shared mesh/count but author depth ahead of the bow
            // camera, not only the inspector's close spherical shell.
            const auto &spawn = profile.flight.hull.position_metres;
            instance.model.columns[12] =
                static_cast<float>(spawn[0] + (instance.model.columns[12] - spawn[0]) * 2);
            instance.model.columns[13] =
                static_cast<float>(spawn[1] + (instance.model.columns[13] - spawn[1]) * 1.5);
            instance.model.columns[14] = static_cast<float>(
                spawn[2] - output.ship_radius * (3 + .12 * static_cast<double>(i)));
        }
        BoxBounds bounds;
        for (const auto &vertex : output.field.vertices)
            bounds.include(
                transformed(instance.model.columns,
                            {vertex.position[0], vertex.position[1], vertex.position[2]}));
        auto &box = output.contacts[i];
        box.dynamic = false;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            box.position_metres[axis] = (bounds.minimum[axis] + bounds.maximum[axis]) * .5;
            box.half_extent_metres[axis] = static_cast<float>(
                std::max(.1, (bounds.maximum[axis] - bounds.minimum[axis]) * .5 + .05));
            if (!std::isfinite(box.position_metres[axis]) ||
                std::abs(box.position_metres[axis]) > 8192 ||
                !std::isfinite(box.half_extent_metres[axis]) || box.half_extent_metres[axis] > 256)
                return render::Failure{"asteroid contact proxy exceeds the local body profile"};
        }
        // Author one clear forward flight lane, not a runtime path planner.
        const auto &spawn = profile.flight.hull.position_metres;
        const auto &h = profile.flight.hull.half_extent_metres;
        if (std::abs(box.position_metres[0] - spawn[0]) <= h[0] + box.half_extent_metres[0] + 10 &&
            std::abs(box.position_metres[1] - spawn[1]) <= h[1] + box.half_extent_metres[1] + 10) {
            const auto x = spawn[0] + std::copysign(h[0] + box.half_extent_metres[0] + 15,
                                                    box.position_metres[0] - spawn[0]);
            const auto old = instance.model.columns[12];
            instance.model.columns[12] += static_cast<float>(x - box.position_metres[0]);
            box.position_metres[0] += static_cast<double>(instance.model.columns[12]) - old;
        }
        const auto &hbox = box.half_extent_metres;
        output.camera_obstacles[i] = {
            box.position_metres,
            std::sqrt(hbox[0] * hbox[0] + hbox[1] * hbox[1] + hbox[2] * hbox[2])};
    }
    std::size_t rcs_count = 0;
    for (std::size_t i = 0; i < package->materials.size(); ++i) {
        const auto &material = package->materials[i];
        if (material.name == "M2_BlueWhiteDrivePlume" && material.alpha == 2)
            output.drive_material = i;
        if (!material.name.starts_with("M3_RCS_"))
            continue;
        if (material.alpha != 2 || material.name.size() != 9 || material.name[7] < '0' ||
            material.name[7] > '9' || material.name[8] < '0' || material.name[8] > '9')
            return render::Failure{"RCS bank requires a named blend material"};
        const auto bank = static_cast<unsigned>(material.name[7] - '0') * 10U +
                          static_cast<unsigned>(material.name[8] - '0');
        if (bank >= rcs_bank_count || output.rcs_materials[bank])
            return render::Failure{"RCS bank index is repeated or out of range"};
        output.rcs_materials[bank] = i;
        ++rcs_count;
    }
    if (rcs_count != 0 && rcs_count != rcs_bank_count)
        return render::Failure{"RCS bank material table is incomplete"};
    if (rcs_count != 0)
        for (const auto material : output.rcs_materials) {
            if (!material)
                return render::Failure{"RCS bank material table is incomplete"};
            const auto index = *material;
            const auto uses = std::count_if(
                package->instances.begin(), package->instances.end(), [&](const auto &instance) {
                    return package->primitives[instance.primitive].material == index;
                });
            if (uses != 1)
                return render::Failure{"RCS bank needs one authored paired-socket instance"};
        }
    return output;
}
} // namespace ngen::demo
