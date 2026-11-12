#include "flight/input.hpp"
#include "flight_demo/preset.hpp"
#include "flight_demo/view.hpp"
#include "package_fixture.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

using namespace ngen;

namespace {
package_fixture::Sections rcs_sections() {
    auto sections = package_fixture::triangle();
    const auto primitive = sections[2];
    const auto instance = sections[8];
    for (std::uint32_t bank = 0; bank < 16; ++bank) {
        auto material = std::vector<std::byte>{};
        for (const auto value : std::array<float, 13>{1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 1, 1, .5F})
            package_fixture::f32(material, value);
        package_fixture::u32(material, 2);
        package_fixture::u32(material, 1);
        for (unsigned map = 0; map < 5; ++map) {
            package_fixture::u32(material, UINT32_MAX);
            package_fixture::u32(material, 0);
        }
        const auto name = std::string("M3_RCS_") + (bank < 10 ? "0" : "") + std::to_string(bank);
        package_fixture::u32(material, static_cast<std::uint32_t>(sections[11].size()));
        package_fixture::u32(material, static_cast<std::uint32_t>(name.size()));
        material.resize(128, std::byte{0});
        sections[3].insert(sections[3].end(), material.begin(), material.end());
        for (const auto character : name)
            sections[11].push_back(static_cast<std::byte>(character));
        auto draw = primitive;
        package_fixture::set32(draw, 8, bank);
        sections[2].insert(sections[2].end(), draw.begin(), draw.end());
        draw = instance;
        package_fixture::set32(draw, 0, bank + 1);
        sections[8].insert(sections[8].end(), draw.begin(), draw.end());
    }
    return sections;
}
} // namespace

TEST_CASE("flight presets bind the complete authored RCS material table") {
    const auto package =
        std::get<assets::PackagePin>(assets::load_package(package_fixture::encode(rcs_sections())));
    demo::ShipProfile profile;
    profile.flight.hull.half_extent_metres = {10, 10, 10};
    const auto result = demo::make_preset(package, profile);
    REQUIRE(std::holds_alternative<demo::Preset>(result));
    const auto &preset = std::get<demo::Preset>(result);
    for (std::size_t bank = 0; bank < 16; ++bank) {
        REQUIRE(preset.rcs_materials[bank] == std::optional<std::size_t>{bank});
    }
    REQUIRE(!preset.drive_material.has_value());
    REQUIRE(package->materials[0].name == "M3_RCS_00");
}

TEST_CASE("flight presets reject partial, duplicate or invalid RCS binding tables atomically") {
    auto sections = rcs_sections();
    SECTION("missing bank") { sections[11][99] = std::byte{'X'}; }
    SECTION("duplicate bank") { sections[11][107] = std::byte{'0'}; }
    SECTION("opaque bank") { package_fixture::set32(sections[3], std::size_t{11} * 128 + 52, 0); }
    SECTION("malformed bank") { sections[11][107] = std::byte{'x'}; }
    SECTION("out of range bank") { sections[11][106] = std::byte{'2'}; }
    SECTION("unused bank") { package_fixture::set32(sections[8], std::size_t{12} * 72, 0); }
    SECTION("two instances use a bank") {
        package_fixture::set32(sections[8], std::size_t{12} * 72, 11);
    }
    const auto package =
        std::get<assets::PackagePin>(assets::load_package(package_fixture::encode(sections)));
    demo::ShipProfile profile;
    profile.flight.hull.half_extent_metres = {10, 10, 10};
    REQUIRE(std::holds_alternative<render::Failure>(demo::make_preset(package, profile)));
}

TEST_CASE("RCS jets respond to actual applied lateral force") {
    const auto result =
        demo::rcs_levels(demo::frigate_profile(), {0, 0, 0, 1}, {12000000, 0, 0}, {});
    REQUIRE(std::holds_alternative<demo::RcsLevels>(result));
    const auto &levels = std::get<demo::RcsLevels>(result);
    REQUIRE(std::count_if(levels.begin(), levels.end(), [](float v) { return v > 0; }) >= 2);
}

TEST_CASE("RCS selection is bounded, owning and covariant with the interval attitude") {
    const auto profile = demo::frigate_profile();
    const std::array<float, 4> identity{0, 0, 0, 1};
    const auto idle = std::get<demo::RcsLevels>(demo::rcs_levels(profile, identity, {}, {}));
    REQUIRE(idle == demo::RcsLevels{});
    const auto forward = std::get<demo::RcsLevels>(
        demo::rcs_levels(profile, identity, {0, 0, -profile.flight.maximum_force_newtons[2]}, {}));
    REQUIRE(forward == idle); // The accepted rear exhaust owns pure forward thrust.
    for (std::size_t axis = 0; axis < 3; ++axis) {
        std::array<float, 3> force{};
        force[axis] = profile.flight.maximum_force_newtons[axis];
        const auto positive =
            std::get<demo::RcsLevels>(demo::rcs_levels(profile, identity, force, {}));
        REQUIRE(positive != idle);
        for (const auto value : positive)
            REQUIRE((std::isfinite(value) && value >= 0 && value <= 1));
        force[axis] = -force[axis];
        const auto negative =
            std::get<demo::RcsLevels>(demo::rcs_levels(profile, identity, force, {}));
        REQUIRE(positive != negative);
        force = {};
        REQUIRE(positive != idle); // Later inputs/results do not mutate the copied output.
    }
    const auto expected = std::get<demo::RcsLevels>(
        demo::rcs_levels(profile, identity, {12000000, 0, 0}, {20000000000.0F, 0, 0}));
    const auto quarter_turn = std::sqrt(.5F);
    const auto rotated = std::get<demo::RcsLevels>(demo::rcs_levels(
        profile, {0, 0, quarter_turn, quarter_turn}, {0, 12000000, 0}, {0, 20000000000.0F, 0}));
    for (std::size_t bank = 0; bank < 16; ++bank)
        REQUIRE(std::abs(expected[bank] - rotated[bank]) < 1e-6F);
    const auto mixed = std::get<demo::RcsLevels>(
        demo::rcs_levels(profile, identity, profile.flight.maximum_force_newtons,
                         profile.flight.maximum_torque_newton_metres));
    for (const auto value : mixed)
        REQUIRE((std::isfinite(value) && value >= 0 && value <= 1));
}

TEST_CASE("pure turns and arrest use selective nonoverlapping RCS sets") {
    const auto profile = demo::frigate_profile();
    for (std::size_t axis = 0; axis < 3; ++axis) {
        std::array<float, 3> torque{};
        torque[axis] = profile.flight.maximum_torque_newton_metres[axis];
        const auto turning =
            std::get<demo::RcsLevels>(demo::rcs_levels(profile, {0, 0, 0, 1}, {}, torque));
        torque[axis] = -torque[axis];
        const auto stopping =
            std::get<demo::RcsLevels>(demo::rcs_levels(profile, {0, 0, 0, 1}, {}, torque));
        REQUIRE(std::count_if(turning.begin(), turning.end(), [](float v) { return v > 1e-6F; }) <=
                6);
        for (std::size_t bank = 0; bank < 16; ++bank)
            REQUIRE((turning[bank] <= 1e-6F || stopping[bank] <= 1e-6F));
    }
}

TEST_CASE("invalid RCS inputs reject before quaternion math or visual publication") {
    auto profile = demo::frigate_profile();
    if (!profile.flight.rcs)
        throw std::logic_error("RCS admission fixture requires a physical rig");
    auto &mix = profile.flight.rcs->mix;
    std::array<float, 4> rotation{0, 0, 0, 1};
    std::array<float, 3> force{}, torque{};
    SECTION("nonfinite force") { force[0] = std::numeric_limits<float>::quiet_NaN(); }
    SECTION("nonfinite torque") { torque[2] = std::numeric_limits<float>::infinity(); }
    SECTION("nonfinite attitude") { rotation[0] = std::numeric_limits<float>::quiet_NaN(); }
    SECTION("zero attitude") { rotation = {}; }
    SECTION("scaled attitude") { rotation[3] = 1.1F; }
    SECTION("zero force limit") { profile.flight.maximum_force_newtons[0] = 0; }
    SECTION("negative torque limit") { profile.flight.maximum_torque_newton_metres[0] = -1; }
    SECTION("nonfinite limit") {
        profile.flight.maximum_torque_newton_metres[1] = std::numeric_limits<float>::quiet_NaN();
    }
    SECTION("nonfinite calibration") { mix[1][3] = std::numeric_limits<float>::quiet_NaN(); }
    SECTION("negative calibration") { mix[0][0] = -.1F; }
    SECTION("unbounded calibration") { mix[0][0] = 1.1F; }
    REQUIRE(std::holds_alternative<render::Failure>(
        demo::rcs_levels(profile, rotation, force, torque)));
}

TEST_CASE("physical turning and released-stick stabilization light different RCS moment pairs") {
    const auto profile = demo::frigate_profile();
    for (std::size_t axis = 0; axis < 3; ++axis) {
        auto game =
            std::get<std::unique_ptr<flight::Flight>>(flight::Flight::create(profile.flight));
        const auto initial = game->snapshot();
        flight::Pilot pilot;
        pilot.rotation[axis] = 1;
        REQUIRE(std::holds_alternative<std::monostate>(
            game->submit({Tick{1}, 1, initial.entity, flight::SetMode{flight::Mode::direct}})));
        REQUIRE(std::holds_alternative<std::monostate>(
            game->submit({Tick{1}, 2, initial.entity, pilot})));
        const auto turning = std::get<flight::Snapshot>(game->step());
        const auto accelerating = turning.rcs_firing;
        REQUIRE(turning.torque_newton_metres[axis] > 0);
        REQUIRE(std::count_if(accelerating.begin(), accelerating.end(),
                              [](float v) { return v > 0; }) >= 2);
        for (unsigned tick = 1; tick < 60; ++tick)
            REQUIRE(std::holds_alternative<flight::Snapshot>(game->step()));
        REQUIRE(std::holds_alternative<std::monostate>(
            game->submit({Tick{61}, 3, initial.entity, flight::Pilot{}})));
        const auto arresting = std::get<flight::Snapshot>(game->step());
        const auto braking = arresting.rcs_firing;
        REQUIRE(arresting.torque_newton_metres[axis] < 0);
        REQUIRE(std::count_if(braking.begin(), braking.end(), [](float v) { return v > 0; }) >= 2);
        REQUIRE(braking != accelerating);
        for (const auto velocity : arresting.body.velocity_mps)
            REQUIRE(std::abs(velocity) < 1e-4F); // Authored float force/moment rounding.
    }
}

TEST_CASE("the authored frigate reaches a useful bounded direct turn rate within one second") {
    const auto ship = demo::frigate_profile().flight;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        auto game = std::get<std::unique_ptr<flight::Flight>>(flight::Flight::create(ship));
        const auto entity = game->snapshot().entity;
        REQUIRE(std::holds_alternative<std::monostate>(
            game->submit({Tick{1}, 1, entity, flight::SetMode{flight::Mode::direct}})));
        flight::Pilot pilot;
        pilot.rotation[axis] = 1;
        REQUIRE(std::holds_alternative<std::monostate>(game->submit({Tick{1}, 2, entity, pilot})));
        for (unsigned tick = 0; tick < 60; ++tick)
            REQUIRE(std::holds_alternative<flight::Snapshot>(game->step()));
        const auto rate = std::abs(game->snapshot().body.angular_velocity_rps[axis]);
        REQUIRE(rate >= .35F);
        REQUIRE(rate <= ship.maximum_angular_speed_rps + .0001F);
    }
}

TEST_CASE("RMB release promptly arrests authored steering through bounded torque without a reset") {
    const auto ship = demo::frigate_profile().flight;
    for (std::size_t axis = 0; axis < 2; ++axis) {
        auto game = std::get<std::unique_ptr<flight::Flight>>(flight::Flight::create(ship));
        const auto entity = game->snapshot().entity;
        flight::FlightInput input;
        REQUIRE(std::holds_alternative<std::monostate>(input.mode(flight::Mode::direct)));
        input.pointer(true);
        REQUIRE(std::holds_alternative<std::monostate>(
            input.motion(axis == 0 ? flight::PointerDelta{0, 250} : flight::PointerDelta{250, 0})));
        REQUIRE(std::holds_alternative<std::monostate>(
            game->submit({Tick{1}, 1, entity, flight::SetMode{flight::Mode::direct}})));
        REQUIRE(std::holds_alternative<std::monostate>(
            game->submit({Tick{1}, 2, entity, input.pilot()})));
        for (unsigned tick = 0; tick < 60; ++tick)
            REQUIRE(std::holds_alternative<flight::Snapshot>(game->step()));
        input.pointer(false);
        REQUIRE(input.pilot().rotation == std::array<float, 3>{});
        REQUIRE(std::holds_alternative<std::monostate>(
            game->submit({Tick{61}, 3, entity, input.pilot()})));
        for (unsigned tick = 0; tick < 18; ++tick) {
            const auto state = std::get<flight::Snapshot>(game->step());
            if (tick == 0)
                REQUIRE(std::abs(state.body.angular_velocity_rps[axis]) > .25F);
            REQUIRE(std::abs(state.torque_newton_metres[axis]) <=
                    ship.maximum_torque_newton_metres[axis] * 1.00001F);
            for (const auto velocity : state.body.velocity_mps)
                REQUIRE(std::abs(velocity) < 1e-4F);
            for (const auto position : state.body.position_metres)
                REQUIRE(std::abs(position) < 1e-4);
            REQUIRE(state.entity == entity);
        }
        REQUIRE(std::abs(game->snapshot().body.angular_velocity_rps[axis]) < .04F);
    }
}

TEST_CASE("visible target markers use GUI-local pixels without moving the shared world origin") {
    demo::Preset preset;
    preset.ship_radius = 10;
    flight::CameraFrame camera;
    camera.ship.position_metres = {100, 200, 300};
    camera.camera.position_metres = {100, 200, 310};
    const auto frame = std::get<demo::FrameView>(demo::present(preset, camera, {1280, 720}));
    const auto pixel = demo::project_target(frame, {100, 200, 300}, {1280, 720});
    REQUIRE(pixel == std::array<float, 2>{640, 360});
    REQUIRE(demo::project_target(frame, {100, 200, 300}, {640, 360}) ==
            std::array<float, 2>{320, 180});
    const auto left = demo::project_target(frame, {99, 200, 300}, {1280, 720});
    REQUIRE(left.has_value());
    if (left)
        REQUIRE((*left)[0] < 640);
    REQUIRE(frame.origin_metres == camera.ship.position_metres);
}

TEST_CASE("invalid marker viewport and homogeneous coordinates publish no pixel") {
    demo::Preset preset;
    preset.ship_radius = 10;
    flight::CameraFrame camera;
    camera.camera.position_metres = {0, 0, 10};
    auto frame = std::get<demo::FrameView>(demo::present(preset, camera, {1280, 720}));
    const std::array<std::array<float, 2>, 4> invalid{
        {{0, 720},
         {1280, 8193},
         {std::numeric_limits<float>::quiet_NaN(), 720},
         {1280, std::numeric_limits<float>::infinity()}}};
    for (const auto size : invalid)
        REQUIRE(!demo::project_target(frame, {0, 0, 0}, size).has_value());
    frame.scene.view_projection.columns[15] = std::numeric_limits<float>::infinity();
    REQUIRE(!demo::project_target(frame, {0, 0, 0}, {1280, 720}).has_value());
}

TEST_CASE("off-screen and camera-plane targets cannot generate unbounded HUD geometry") {
    demo::Preset preset;
    preset.ship_radius = 10;
    flight::CameraFrame camera;
    camera.ship.position_metres = {100, 200, 300};
    camera.camera.position_metres = {100, 200, 310};
    const auto frame = std::get<demo::FrameView>(demo::present(preset, camera, {1280, 720}));
    const std::array<std::array<double, 3>, 7> outside{{{2000, 200, 309.8},
                                                        {-2000, 200, 309.8},
                                                        {100, 2000, 309.8},
                                                        {100, -2000, 309.8},
                                                        {100, 200, 311},
                                                        {100, 200, 309.95},
                                                        {100, 200, -10000}}};
    for (const auto &point : outside)
        REQUIRE(!demo::project_target(frame, point, {1280, 720}).has_value());
}

TEST_CASE("rotated ship roots and camera axes share a double-position presentation origin") {
    const auto package =
        std::get<assets::PackagePin>(assets::load_package(package_fixture::encode()));
    demo::ShipProfile profile;
    profile.flight.hull.half_extent_metres = {10, 10, 10};
    profile.asset_to_body.columns[12] = -4;
    profile.asset_to_body.columns[13] = -6;
    profile.asset_to_body.columns[14] = -6;
    const auto preset = std::get<demo::Preset>(demo::make_preset(package, profile));
    flight::CameraFrame camera;
    camera.ship.position_metres = {4096.125, 500.5, -2000.75};
    camera.ship.rotation_xyzw = {0, std::sqrt(.5F), 0, std::sqrt(.5F)};
    camera.camera = camera.ship;
    camera.camera.position_metres[0] += 10;
    const auto result = demo::present(preset, camera, {1280, 720});
    REQUIRE(std::holds_alternative<demo::FrameView>(result));
    const auto &view = std::get<demo::FrameView>(result);
    REQUIRE(view.origin_metres == camera.ship.position_metres);
    REQUIRE(view.scene.camera_position == render::shader::Float4{10, 0, 0, 1});
    const auto &root = view.scene.root_transform.columns;
    REQUIRE(std::abs(root[12] + 6) < 1e-5F);
    REQUIRE(std::abs(root[13] + 6) < 1e-5F);
    REQUIRE(std::abs(root[14] - 4) < 1e-5F);
    const auto &projection = view.scene.view_projection.columns;
    REQUIRE(std::abs(projection[12]) < 1e-5F);
    REQUIRE(std::abs(projection[13]) < 1e-5F);
    REQUIRE(std::abs(projection[15] - 10) < 1e-5F);
    REQUIRE(projection[14] > 0);
    REQUIRE(projection[14] < projection[15]);
    REQUIRE(camera.ship.position_metres[0] == 4096.125);
}

TEST_CASE("invalid camera values reject before native quaternion math or frame publication") {
    const auto package =
        std::get<assets::PackagePin>(assets::load_package(package_fixture::encode()));
    demo::ShipProfile profile;
    profile.flight.hull.half_extent_metres = {10, 10, 10};
    profile.asset_to_body.columns[12] = -4;
    profile.asset_to_body.columns[13] = -6;
    profile.asset_to_body.columns[14] = -6;
    const auto preset = std::get<demo::Preset>(demo::make_preset(package, profile));
    flight::CameraFrame camera;
    REQUIRE(std::holds_alternative<render::Failure>(demo::present(preset, camera, {0, 720})));
    REQUIRE(std::holds_alternative<render::Failure>(demo::present(preset, camera, {8193, 720})));
    for (const auto value : {29.0F, 111.0F, std::numeric_limits<float>::quiet_NaN()}) {
        camera.vertical_fov_degrees = value;
        REQUIRE(
            std::holds_alternative<render::Failure>(demo::present(preset, camera, {1280, 720})));
    }
    camera = {};
    camera.camera.rotation_xyzw = {0, 0, 0, 0};
    REQUIRE(std::holds_alternative<render::Failure>(demo::present(preset, camera, {1280, 720})));
    camera = {};
    camera.ship.position_metres[0] = 16385;
    REQUIRE(std::holds_alternative<render::Failure>(demo::present(preset, camera, {1280, 720})));
}

TEST_CASE("the playable preset copies authored ship data and shares one hundred bounded asteroid "
          "proxies") {
    auto loaded = assets::load_package(package_fixture::encode());
    REQUIRE(std::holds_alternative<assets::PackagePin>(loaded));
    auto package = std::get<assets::PackagePin>(loaded);
    demo::ShipProfile profile;
    profile.flight.hull.half_extent_metres = {1.2F, 1.2F, .5F};
    profile.flight.flight_camera = {{.2, .4, -.6}, {0, 0, 0, 1}, 80};
    profile.asset_to_body.columns[12] = -4;
    profile.asset_to_body.columns[13] = -6;
    profile.asset_to_body.columns[14] = -6;
    const auto result = demo::make_preset(package, profile);
    REQUIRE(std::holds_alternative<demo::Preset>(result));
    const auto &preset = std::get<demo::Preset>(result);
    REQUIRE(preset.ship.flight.flight_camera.position_metres ==
            profile.flight.flight_camera.position_metres);
    REQUIRE(preset.ship.flight.flight_camera.vertical_fov_degrees == 80);
    REQUIRE(preset.triangles == 8001);
    REQUIRE(preset.field.instances.size() == 100);
    std::size_t ahead = 0;
    for (const auto &box : preset.contacts)
        if (box.position_metres[2] < -3 * preset.ship_radius)
            ++ahead;
    REQUIRE(ahead >= 20);
    for (std::size_t i = 0; i < preset.contacts.size(); ++i) {
        const auto &box = preset.contacts[i];
        REQUIRE(!box.dynamic);
        REQUIRE(box.half_extent_metres[0] >= .1F);
        REQUIRE(box.half_extent_metres[0] <= 256);
        REQUIRE(preset.camera_obstacles[i].centre_metres == box.position_metres);
        REQUIRE(preset.camera_obstacles[i].radius_metres > 0);
        const auto outside_lane =
            std::abs(box.position_metres[0]) >
                profile.flight.hull.half_extent_metres[0] + box.half_extent_metres[0] ||
            std::abs(box.position_metres[1]) >
                profile.flight.hull.half_extent_metres[1] + box.half_extent_metres[1];
        REQUIRE(outside_lane);
    }
    REQUIRE(std::holds_alternative<std::unique_ptr<flight::Flight>>(
        flight::Flight::create(preset.ship.flight, preset.contacts)));
}

TEST_CASE("flight presentation uses a camera-relative origin and perspective without moving static "
          "world objects") {
    const auto package =
        std::get<assets::PackagePin>(assets::load_package(package_fixture::encode()));
    demo::ShipProfile profile;
    profile.flight.hull.half_extent_metres = {1.2F, 1.2F, .5F};
    profile.asset_to_body.columns[12] = -4;
    profile.asset_to_body.columns[13] = -6;
    profile.asset_to_body.columns[14] = -6;
    const auto preset = std::get<demo::Preset>(demo::make_preset(package, profile));
    flight::CameraFrame camera;
    camera.ship.position_metres = {100, 200, 300};
    camera.camera.position_metres = {100, 200, 310};
    const auto result = demo::present(preset, camera, {1280, 720});
    REQUIRE(std::holds_alternative<demo::FrameView>(result));
    const auto &frame = std::get<demo::FrameView>(result);
    REQUIRE(frame.origin_metres == camera.ship.position_metres);
    REQUIRE(frame.scene.camera_position == render::shader::Float4{0, 0, 10, 1});
    REQUIRE(frame.scene.root_transform.columns[12] == -4);
    REQUIRE(frame.scene.root_transform.columns[13] == -6);
    REQUIRE(frame.scene.root_transform.columns[14] == -6);
    for (std::size_t axis = 0; axis < 3; ++axis)
        REQUIRE(std::abs(frame.asteroids[0].model.columns[12 + axis] -
                         (preset.field.instances[0].model.columns[12 + axis] -
                          camera.ship.position_metres[axis])) < .0001);
    const auto &projection = frame.scene.view_projection.columns;
    REQUIRE(std::abs(projection[12]) < 1e-5F);
    REQUIRE(std::abs(projection[13]) < 1e-5F);
    REQUIRE(projection[14] > 0);
    REQUIRE(projection[14] < projection[15]);
    REQUIRE(std::abs(projection[15] - 10) < 1e-5F);
    REQUIRE(preset.field.instances[0].model.columns[12] != frame.asteroids[0].model.columns[12]);
    REQUIRE(camera.ship.position_metres == std::array<double, 3>{100, 200, 300});
}

TEST_CASE(
    "an authored ship-to-body root cannot shrink, mirror, shear or project its immutable package") {
    const auto package =
        std::get<assets::PackagePin>(assets::load_package(package_fixture::encode()));
    demo::ShipProfile profile;
    profile.flight.hull.half_extent_metres = {10, 10, 10};
    profile.asset_to_body.columns[12] = -4;
    profile.asset_to_body.columns[13] = -6;
    profile.asset_to_body.columns[14] = -6;
    REQUIRE(std::holds_alternative<demo::Preset>(demo::make_preset(package, profile)));
    const auto original = profile.asset_to_body;
    const std::array<std::pair<std::size_t, float>, 7> invalid{
        {{0, .5F},
         {0, -1.0F},
         {4, .25F},
         {3, .25F},
         {15, 0.0F},
         {7, std::numeric_limits<float>::quiet_NaN()},
         {14, std::numeric_limits<float>::infinity()}}};
    for (const auto &[index, value] : invalid) {
        profile.asset_to_body = original;
        profile.asset_to_body.columns[index] = value;
        REQUIRE(std::holds_alternative<render::Failure>(demo::make_preset(package, profile)));
    }
}
