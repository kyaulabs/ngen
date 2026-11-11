#include "flight/input.hpp"
#include "flight_demo/preset.hpp"
#include "flight_demo/view.hpp"
#include "package_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <utility>

using namespace ngen;

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
            REQUIRE(state.body.velocity_mps == std::array<float, 3>{});
            REQUIRE(state.body.position_metres == std::array<double, 3>{});
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
         {0, -1},
         {4, .25F},
         {3, .25F},
         {15, 0},
         {7, std::numeric_limits<float>::quiet_NaN()},
         {14, std::numeric_limits<float>::infinity()}}};
    for (const auto &[index, value] : invalid) {
        profile.asset_to_body = original;
        profile.asset_to_body.columns[index] = value;
        REQUIRE(std::holds_alternative<render::Failure>(demo::make_preset(package, profile)));
    }
}
