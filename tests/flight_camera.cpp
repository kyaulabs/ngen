#include "flight/camera.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>

using namespace ngen;
using namespace ngen::flight;

TEST_CASE("first person composes each ship's authored local mount instead of a universal offset") {
    auto rig = std::get<CameraRig>(CameraRig::create(3));
    Snapshot state;
    state.entity = {0, 1};
    state.mode = Mode::direct;
    state.body.position_metres = {100, 200, 300};
    const auto half = std::sqrt(.5F);
    state.body.rotation_xyzw = {0, half, 0, half};
    state.flight_camera.position_metres = {2, 3, 4};
    state.flight_camera.rotation_xyzw = {0, half, 0, half};
    const auto first = std::get<CameraFrame>(rig.sample({state, state}, .5F));
    REQUIRE(first.camera.position_metres[0] == Catch::Approx(104).margin(1e-5));
    REQUIRE(first.camera.position_metres[1] == Catch::Approx(203).margin(1e-5));
    REQUIRE(first.camera.position_metres[2] == Catch::Approx(298).margin(1e-5));
    REQUIRE(std::abs(first.camera.rotation_xyzw[1]) == Catch::Approx(1).margin(1e-5));
    REQUIRE(std::abs(first.camera.rotation_xyzw[3]) < 1e-5F);
    state.flight_camera.position_metres = {-5, 1, 0};
    state.flight_camera.rotation_xyzw = {0, 0, 0, 1};
    state.flight_camera.vertical_fov_degrees = 85;
    const auto second = std::get<CameraFrame>(rig.sample({state, state}, .5F));
    REQUIRE(second.camera.position_metres[0] == Catch::Approx(100).margin(1e-5));
    REQUIRE(second.camera.position_metres[1] == Catch::Approx(201).margin(1e-5));
    REQUIRE(second.camera.position_metres[2] == Catch::Approx(305).margin(1e-5));
    REQUIRE(second.vertical_fov_degrees == 85);
    REQUIRE(first.camera.position_metres[0] == Catch::Approx(104).margin(1e-5));
    REQUIRE(state.body.position_metres == std::array<double, 3>{100, 200, 300});
}

TEST_CASE(
    "presentation interpolates translation and shortest-path attitude before applying the mount") {
    auto rig = std::get<CameraRig>(CameraRig::create(3));
    Snapshot before, current;
    before.entity = current.entity = {0, 1};
    before.mode = current.mode = Mode::direct;
    before.tick = Tick{1};
    current.tick = Tick{2};
    current.body.position_metres = {10, 0, 0};
    current.body.rotation_xyzw = {0, 1, 0, 0};
    current.flight_camera.position_metres = {0, 0, -2};
    const auto middle = std::get<CameraFrame>(rig.sample({before, current}, .5F));
    REQUIRE(middle.ship.position_metres[0] == Catch::Approx(5));
    REQUIRE(middle.ship.rotation_xyzw[1] == Catch::Approx(std::sqrt(.5F)).margin(1e-5));
    REQUIRE(middle.camera.position_metres[0] == Catch::Approx(3).margin(1e-5));
    REQUIRE(std::abs(middle.camera.position_metres[2]) < 1e-5);
    current.body.rotation_xyzw = {0, 0, 0, -1};
    const auto equivalent = std::get<CameraFrame>(rig.sample({before, current}, .5F));
    REQUIRE(std::abs(equivalent.ship.rotation_xyzw[3]) == Catch::Approx(1));
    REQUIRE(equivalent.camera.position_metres[2] == Catch::Approx(-2).margin(1e-5));
    REQUIRE(current.body.position_metres[0] == 10);
    REQUIRE(before.body.position_metres[0] == 0);
}

TEST_CASE("the default tactical orbit looks at the ship without inheriting hull attitude") {
    auto rig = std::get<CameraRig>(CameraRig::create(3));
    Snapshot state;
    state.entity = {0, 1};
    state.body.position_metres = {10, 20, 30};
    const auto half = std::sqrt(.5F);
    state.body.rotation_xyzw = {0, half, 0, half};
    const auto view = std::get<CameraFrame>(rig.sample({state, state}, 0));
    const auto &p = view.camera.position_metres;
    const auto &anchor = view.ship.position_metres;
    const auto dx = anchor[0] - p[0], dy = anchor[1] - p[1], dz = anchor[2] - p[2];
    const auto distance = std::sqrt(dx * dx + dy * dy + dz * dz);
    const auto &q = view.camera.rotation_xyzw;
    const std::array<double, 3> forward{-2 * (q[0] * q[2] + q[3] * q[1]),
                                        -2 * (q[1] * q[2] - q[3] * q[0]),
                                        -(1 - 2 * (q[0] * q[0] + q[1] * q[1]))};
    REQUIRE((forward[0] * dx + forward[1] * dy + forward[2] * dz) / distance > .99999);
    REQUIRE(distance == Catch::Approx(9).margin(1e-5));
    REQUIRE(p[1] > anchor[1]);
    REQUIRE(view.ship.position_metres == state.body.position_metres);
}

TEST_CASE(
    "tactical drag and wheel deltas orbit and zoom within finite bounds without moving the ship") {
    auto rig = std::get<CameraRig>(CameraRig::create(3));
    Snapshot state;
    state.entity = {0, 1};
    const auto initial = std::get<CameraFrame>(rig.sample({state, state}, 0));
    REQUIRE(std::holds_alternative<std::monostate>(rig.orbit({.4F, .2F, 4})));
    const auto changed = std::get<CameraFrame>(rig.sample({state, state}, 0));
    const auto distance = [](const Pose &pose) {
        const auto &p = pose.position_metres;
        return std::sqrt(p[0] * p[0] + p[1] * p[1] + p[2] * p[2]);
    };
    REQUIRE(changed.camera.position_metres != initial.camera.position_metres);
    REQUIRE(distance(changed.camera) == Catch::Approx(9 * std::exp(-.6)).margin(1e-5));
    REQUIRE(changed.ship.position_metres == std::array<double, 3>{});
    for (unsigned i = 0; i < 50; ++i)
        REQUIRE(std::holds_alternative<std::monostate>(rig.orbit({3, 3, 16})));
    const auto near = std::get<CameraFrame>(rig.sample({state, state}, 0));
    REQUIRE(distance(near.camera) >= 3.49999);
    for (unsigned i = 0; i < 50; ++i)
        REQUIRE(std::holds_alternative<std::monostate>(rig.orbit({-3, -3, -16})));
    const auto far = std::get<CameraFrame>(rig.sample({state, state}, 0));
    REQUIRE(distance(far.camera) <= 4096.001);
    REQUIRE(std::abs(far.camera.position_metres[1]) / distance(far.camera) < .98);
}

TEST_CASE("tactical focus copies its target while first person remains at the ship's mount") {
    auto rig = std::get<CameraRig>(CameraRig::create(3));
    Snapshot state;
    state.entity = {0, 1};
    state.flight_camera.position_metres = {1, 2, 3};
    CameraSphere target{{100, 10, 0}, 10};
    REQUIRE(std::holds_alternative<std::monostate>(rig.focus(target)));
    const std::array focused_obstacle{target};
    target.centre_metres = {};
    const auto tactical = std::get<CameraFrame>(rig.sample({state, state}, 0, focused_obstacle));
    const auto &p = tactical.camera.position_metres;
    REQUIRE(std::sqrt((p[0] - 100) * (p[0] - 100) + (p[1] - 10) * (p[1] - 10) + p[2] * p[2]) ==
            Catch::Approx(30).margin(1e-4));
    state.mode = Mode::direct;
    const auto direct = std::get<CameraFrame>(rig.sample({state, state}, 0));
    REQUIRE(direct.camera.position_metres == state.flight_camera.position_metres);
    REQUIRE(direct.ship.position_metres == std::array<double, 3>{});
    REQUIRE(std::holds_alternative<std::monostate>(rig.focus(std::nullopt)));
    state.mode = Mode::tactical;
    const auto follow = std::get<CameraFrame>(rig.sample({state, state}, 0));
    const auto &f = follow.camera.position_metres;
    REQUIRE(std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]) == Catch::Approx(9).margin(1e-5));
}

TEST_CASE("bounded first-person free look rotates around the mount and centers without steering "
          "the ship") {
    auto rig = std::get<CameraRig>(CameraRig::create(3));
    Snapshot state;
    state.entity = {0, 1};
    state.mode = Mode::direct;
    state.flight_camera.position_metres = {1, 2, 3};
    const auto original = std::get<CameraFrame>(rig.sample({state, state}, 0));
    REQUIRE(std::holds_alternative<std::monostate>(rig.look({.5F, .25F, 0})));
    const auto looked = std::get<CameraFrame>(rig.sample({state, state}, 0));
    REQUIRE(looked.camera.rotation_xyzw != original.camera.rotation_xyzw);
    REQUIRE(looked.camera.position_metres == original.camera.position_metres);
    REQUIRE(looked.ship.rotation_xyzw == state.body.rotation_xyzw);
    REQUIRE(looked.vertical_fov_degrees == original.vertical_fov_degrees);
    rig.center_look();
    const auto centered = std::get<CameraFrame>(rig.sample({state, state}, 0));
    for (std::size_t i = 0; i < 4; ++i)
        REQUIRE(centered.camera.rotation_xyzw[i] ==
                Catch::Approx(original.camera.rotation_xyzw[i]));
    REQUIRE(centered.camera.position_metres == original.camera.position_metres);
}

TEST_CASE(
    "a tactical camera boom shortens before an obstacle without pushing the mounted first person") {
    auto rig = std::get<CameraRig>(CameraRig::create(3));
    Snapshot state;
    state.entity = {0, 1};
    const auto original = std::get<CameraFrame>(rig.sample({state, state}, 0));
    const auto &p = original.camera.position_metres;
    const CameraSphere obstacle{{p[0] * 2 / 3, p[1] * 2 / 3, p[2] * 2 / 3}, 1};
    const std::array obstacles{obstacle};
    const auto safe = std::get<CameraFrame>(rig.sample({state, state}, 0, obstacles));
    const auto &eye = safe.camera.position_metres;
    const auto distance = std::sqrt(eye[0] * eye[0] + eye[1] * eye[1] + eye[2] * eye[2]);
    REQUIRE(distance < 4.751);
    REQUIRE(distance > 3.499);
    const auto &c = obstacle.centre_metres;
    REQUIRE(std::sqrt((eye[0] - c[0]) * (eye[0] - c[0]) + (eye[1] - c[1]) * (eye[1] - c[1]) +
                      (eye[2] - c[2]) * (eye[2] - c[2])) >= 1.2499);
    state.mode = Mode::direct;
    state.flight_camera.position_metres = {1, 2, 3};
    const auto mounted = std::get<CameraFrame>(rig.sample({state, state}, 0, obstacles));
    REQUIRE(mounted.camera.position_metres == state.flight_camera.position_metres);
    REQUIRE(state.body.position_metres == std::array<double, 3>{});
}

TEST_CASE(
    "a near-contact blocked boom tries bounded alternative angles rather than entering the hull") {
    auto rig = std::get<CameraRig>(CameraRig::create(3));
    Snapshot state;
    state.entity = {0, 1};
    const auto original = std::get<CameraFrame>(rig.sample({state, state}, 0));
    const auto &p = original.camera.position_metres;
    const CameraSphere obstacle{{p[0] * .5, p[1] * .5, p[2] * .5}, 1.5F};
    const std::array obstacles{obstacle};
    const auto result = rig.sample({state, state}, 0, obstacles);
    REQUIRE(std::holds_alternative<CameraFrame>(result));
    const auto &eye = std::get<CameraFrame>(result).camera.position_metres;
    REQUIRE(std::sqrt(eye[0] * eye[0] + eye[1] * eye[1] + eye[2] * eye[2]) >= 3.4999);
    const auto &c = obstacle.centre_metres;
    REQUIRE(std::sqrt((eye[0] - c[0]) * (eye[0] - c[0]) + (eye[1] - c[1]) * (eye[1] - c[1]) +
                      (eye[2] - c[2]) * (eye[2] - c[2])) >= 1.7499);
}

TEST_CASE(
    "invalid camera inputs preserve the rig and bounded unsatisfiable views fail explicitly") {
    REQUIRE(std::get<Error>(CameraRig::create(0)) == Error::invalid);
    auto rig = std::get<CameraRig>(CameraRig::create(3));
    Snapshot state;
    state.entity = {0, 1};
    const auto original = std::get<CameraFrame>(rig.sample({state, state}, 0));
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    REQUIRE(std::get<Error>(rig.orbit({nan, 0, 0})) == Error::invalid);
    REQUIRE(std::get<Error>(rig.look({0, 0, 1})) == Error::invalid);
    REQUIRE(std::get<Error>(rig.focus(CameraSphere{{}, nan})) == Error::invalid);
    REQUIRE(std::get<Error>(rig.sample({state, state}, nan)) == Error::invalid);
    REQUIRE(std::get<Error>(rig.sample({state, state}, 2)) == Error::invalid);
    const auto preserved = std::get<CameraFrame>(rig.sample({state, state}, 0));
    REQUIRE(preserved.camera.position_metres == original.camera.position_metres);
    REQUIRE(preserved.camera.rotation_xyzw == original.camera.rotation_xyzw);
    state.flight_camera.rotation_xyzw = {};
    REQUIRE(std::get<Error>(rig.sample({state, state}, 0)) == Error::invalid);
    state.flight_camera.rotation_xyzw = {0, 0, 0, 1};
    const std::array<CameraSphere, physics::World::body_capacity + 1> too_many{};
    REQUIRE(std::get<Error>(rig.sample({state, state}, 0, too_many)) == Error::invalid);
    const std::array enclosed{CameraSphere{{}, 100}};
    REQUIRE(std::get<Error>(rig.sample({state, state}, 0, enclosed)) == Error::busy);
}

TEST_CASE("reset epochs and missing tick intervals snap instead of interpolating across scenes") {
    auto rig = std::get<CameraRig>(CameraRig::create(3));
    Snapshot old, current;
    old.entity = {0, 1};
    old.tick = Tick{10};
    old.body.rotation_xyzw = {0, 1, 0, 0};
    current.entity = {0, 2};
    current.mode = Mode::direct;
    current.body.position_metres = {100, 0, 0};
    current.flight_camera.position_metres = {0, 0, -2};
    const auto reset = std::get<CameraFrame>(rig.sample({old, current}, .25F));
    REQUIRE(reset.camera.position_metres == std::array<double, 3>{100, 0, -2});
    REQUIRE(reset.ship.rotation_xyzw == std::array<float, 4>{0, 0, 0, 1});
    current.entity = old.entity;
    current.tick = Tick{20};
    const auto gap = std::get<CameraFrame>(rig.sample({old, current}, .25F));
    REQUIRE(gap.camera.position_metres == reset.camera.position_metres);
}

TEST_CASE(
    "sampling a different presentation cadence has no hidden lag or simulation side effects") {
    auto rig = std::get<CameraRig>(CameraRig::create(3));
    Snapshot previous, current;
    previous.entity = current.entity = {0, 1};
    previous.tick = Tick{1};
    current.tick = Tick{2};
    current.mode = Mode::direct;
    current.body.position_metres = {20, 10, -10};
    current.body.rotation_xyzw = {0, 1, 0, 0};
    current.flight_camera.position_metres = {1, 2, 3};
    const auto expected = std::get<CameraFrame>(rig.sample({previous, current}, .8F));
    for (const auto fraction : {.1F, .6F, .2F, 1.0F, 0.0F})
        REQUIRE(std::holds_alternative<CameraFrame>(rig.sample({previous, current}, fraction)));
    const auto repeated = std::get<CameraFrame>(rig.sample({previous, current}, .8F));
    REQUIRE(repeated.ship.position_metres == expected.ship.position_metres);
    REQUIRE(repeated.ship.rotation_xyzw == expected.ship.rotation_xyzw);
    REQUIRE(repeated.camera.position_metres == expected.camera.position_metres);
    REQUIRE(repeated.camera.rotation_xyzw == expected.camera.rotation_xyzw);
    REQUIRE(current.body.position_metres == std::array<double, 3>{20, 10, -10});
    REQUIRE(previous.body.position_metres == std::array<double, 3>{});
}

TEST_CASE("a presentation mount outside the admitted per-ship range cannot teleport the eye") {
    auto rig = std::get<CameraRig>(CameraRig::create(3));
    Snapshot state;
    state.entity = {0, 1};
    state.mode = Mode::direct;
    state.flight_camera.position_metres = {2000, 0, 0};
    const auto result = rig.sample({state, state}, 0);
    REQUIRE(std::holds_alternative<Error>(result));
    REQUIRE(std::get<Error>(result) == Error::invalid);
}

TEST_CASE("orbiting a selected target also keeps the tactical eye outside the controlled hull") {
    auto rig = std::get<CameraRig>(CameraRig::create(3));
    Snapshot state;
    state.entity = {0, 1};
    REQUIRE(std::holds_alternative<std::monostate>(rig.focus(CameraSphere{{100, 0, 0}, 10})));
    const auto initial = std::get<CameraFrame>(rig.sample({state, state}, 0));
    state.body.position_metres = initial.camera.position_metres;
    const auto result = rig.sample({state, state}, 0);
    REQUIRE(std::holds_alternative<CameraFrame>(result));
    const auto &eye = std::get<CameraFrame>(result).camera.position_metres;
    const auto &ship = state.body.position_metres;
    const auto dx = eye[0] - ship[0], dy = eye[1] - ship[1], dz = eye[2] - ship[2];
    REQUIRE(std::sqrt(dx * dx + dy * dy + dz * dz) >= 3.2499);
}
