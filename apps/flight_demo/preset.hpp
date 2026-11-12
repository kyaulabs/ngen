#pragma once
#include "flight/camera.hpp"
#include "scene_workload/fixture.hpp"
#include <optional>

namespace ngen::demo {
inline constexpr std::size_t rcs_bank_count = 16;
using RcsLevels = std::array<float, rcs_bank_count>;
// Columns are +Fx, -Fx, +Fy, -Fy, +Fz, -Fz, +Tx, -Tx, +Ty, -Ty, +Tz, -Tz.
using RcsMix = std::array<std::array<float, 12>, rcs_bank_count>;
struct ShipProfile {
    flight::ShipDefinition flight;
    std::uint64_t root_padding{}; // Existing alignment gap before the copied matrix.
    render::shader::Matrix asset_to_body{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    std::uint32_t expected_package_crc{}; // Compatibility/corruption marker, not authentication.
    std::array<std::uint32_t, 3> profile_padding{};
    RcsMix rcs_mix{};
};
struct Preset {
    ShipProfile ship;
    workload::Fixture field;
    std::array<physics::Box, 100> contacts;
    std::array<flight::CameraSphere, 100> camera_obstacles;
    float ship_radius{};
    std::uint64_t triangles{};
    std::optional<std::size_t> drive_material;
    std::array<std::optional<std::size_t>, rcs_bank_count> rcs_materials{};
};
[[nodiscard]] ShipProfile frigate_profile();
// Applied world wrench and its before-tick attitude; never pilot intent or end attitude.
[[nodiscard]] std::variant<RcsLevels, render::Failure>
rcs_levels(const ShipProfile &profile, const std::array<float, 4> &interval_rotation_xyzw,
           const std::array<float, 3> &world_force, const std::array<float, 3> &world_torque);
[[nodiscard]] std::variant<Preset, render::Failure> make_preset(const assets::PackagePin &package,
                                                                const ShipProfile &profile);
} // namespace ngen::demo
