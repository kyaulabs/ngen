#include "flight/input.hpp"
#include <bit>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace ngen;
using namespace ngen::flight;

TEST_CASE("direct held controls map to body-local six-axis intent and release coasts") {
    FlightInput input;
    REQUIRE(std::holds_alternative<std::monostate>(input.mode(Mode::direct)));
    for (const auto key : {FlightKey::forward, FlightKey::right, FlightKey::up, FlightKey::pitch_up,
                           FlightKey::yaw_left, FlightKey::roll_left})
        REQUIRE(std::holds_alternative<std::monostate>(input.key(key, true)));
    const auto held = input.pilot();
    REQUIRE(held.thrust == std::array<float, 3>{1, 1, -1});
    REQUIRE(held.rotation == std::array<float, 3>{1, 1, 1});
    REQUIRE(!held.brake);
    REQUIRE(std::holds_alternative<std::monostate>(input.key(FlightKey::forward, false)));
    REQUIRE(input.pilot().thrust[2] == 0);
    REQUIRE(std::holds_alternative<std::monostate>(input.key(FlightKey::brake, true)));
    REQUIRE(input.pilot().brake);
}

TEST_CASE("focus loss clears held thrust and capture without replaying them when focus returns") {
    FlightInput input;
    REQUIRE(std::holds_alternative<std::monostate>(input.mode(Mode::direct)));
    REQUIRE(std::holds_alternative<std::monostate>(input.key(FlightKey::forward, true)));
    REQUIRE(std::holds_alternative<std::monostate>(input.key(FlightKey::brake, true)));
    input.pointer(true);
    REQUIRE(input.capture());
    REQUIRE(input.pilot().thrust[2] == -1);
    input.focus(false);
    REQUIRE(!input.capture());
    REQUIRE(input.pilot().thrust == std::array<float, 3>{});
    REQUIRE(!input.pilot().brake);
    input.focus(true);
    REQUIRE(!input.capture());
    REQUIRE(input.pilot().thrust == std::array<float, 3>{});
    REQUIRE(!input.pilot().brake);
    REQUIRE(input.take_camera().center_look);
    REQUIRE(!input.take_camera().center_look);
}

TEST_CASE(
    "captured direct mouse intent is a bounded virtual stick independent of sampling cadence") {
    FlightInput input;
    REQUIRE(std::holds_alternative<std::monostate>(input.mode(Mode::direct)));
    input.pointer(true);
    REQUIRE(std::holds_alternative<std::monostate>(input.motion({100, -50})));
    const auto target = input.pilot();
    REQUIRE(target.rotation[0] == Catch::Approx(.2F));
    REQUIRE(target.rotation[1] == Catch::Approx(-.4F));
    for (unsigned i = 0; i < 10; ++i)
        REQUIRE(input.pilot().rotation == target.rotation);
    REQUIRE(std::holds_alternative<std::monostate>(input.motion({2000, -2000})));
    REQUIRE(input.pilot().rotation[0] == 1);
    REQUIRE(input.pilot().rotation[1] == -1);
    input.pointer(false);
    REQUIRE(!input.capture());
    REQUIRE(input.pilot().rotation == std::array<float, 3>{});
    REQUIRE(input.take_camera().center_look);
}

TEST_CASE("tactical drag and wheel emit camera deltas once rather than ship steering") {
    FlightInput input;
    input.pointer(true);
    REQUIRE(std::holds_alternative<std::monostate>(input.motion({100, -50})));
    REQUIRE(std::holds_alternative<std::monostate>(input.wheel(2)));
    const auto events = input.take_camera();
    REQUIRE(events.orbit.yaw_radians == Catch::Approx(-.3F));
    REQUIRE(events.orbit.pitch_radians == Catch::Approx(-.15F));
    REQUIRE(events.orbit.zoom_steps == 2);
    REQUIRE(input.pilot().rotation == std::array<float, 3>{});
    REQUIRE(input.pilot().thrust == std::array<float, 3>{});
    const auto consumed = input.take_camera();
    REQUIRE(consumed.orbit.yaw_radians == 0);
    REQUIRE(consumed.orbit.pitch_radians == 0);
    REQUIRE(consumed.orbit.zoom_steps == 0);
}

TEST_CASE(
    "free look releases the steering stick and centers on release without resuming stale turns") {
    FlightInput input;
    REQUIRE(std::holds_alternative<std::monostate>(input.mode(Mode::direct)));
    input.pointer(true);
    REQUIRE(std::holds_alternative<std::monostate>(input.motion({100, 0})));
    REQUIRE(input.pilot().rotation[1] == Catch::Approx(-.4F));
    REQUIRE(std::holds_alternative<std::monostate>(input.key(FlightKey::free_look, true)));
    REQUIRE(input.pilot().rotation == std::array<float, 3>{});
    REQUIRE(std::holds_alternative<std::monostate>(input.motion({100, -50})));
    const auto looking = input.take_camera();
    REQUIRE(looking.look.yaw_radians == Catch::Approx(-.3F));
    REQUIRE(looking.look.pitch_radians == Catch::Approx(-.15F));
    REQUIRE(input.pilot().rotation == std::array<float, 3>{});
    REQUIRE(std::holds_alternative<std::monostate>(input.key(FlightKey::free_look, false)));
    REQUIRE(input.take_camera().center_look);
    REQUIRE(input.pilot().rotation == std::array<float, 3>{});
    REQUIRE(input.capture());
    REQUIRE(std::holds_alternative<std::monostate>(input.motion({50, 0})));
    REQUIRE(input.pilot().rotation[1] == Catch::Approx(-.2F));
}

TEST_CASE("a view-mode change releases live input but confirming the current mode does not") {
    FlightInput input;
    REQUIRE(std::holds_alternative<std::monostate>(input.mode(Mode::direct)));
    REQUIRE(std::holds_alternative<std::monostate>(input.key(FlightKey::forward, true)));
    input.pointer(true);
    REQUIRE(std::holds_alternative<std::monostate>(input.motion({100, 0})));
    REQUIRE(std::holds_alternative<std::monostate>(input.mode(Mode::direct)));
    REQUIRE(input.pilot().thrust[2] == -1);
    REQUIRE(input.capture());
    REQUIRE(std::holds_alternative<std::monostate>(input.mode(Mode::tactical)));
    REQUIRE(!input.capture());
    REQUIRE(input.pilot().thrust == std::array<float, 3>{});
    REQUIRE(std::holds_alternative<std::monostate>(input.mode(Mode::direct)));
    REQUIRE(input.pilot().thrust == std::array<float, 3>{});
    REQUIRE(input.pilot().rotation == std::array<float, 3>{});
    REQUIRE(!input.capture());
    REQUIRE(input.take_camera().center_look);
}

TEST_CASE("invalid and unfocused events do not mutate or secretly queue live input") {
    FlightInput input;
    REQUIRE(std::holds_alternative<std::monostate>(input.mode(Mode::direct)));
    REQUIRE(std::holds_alternative<std::monostate>(input.key(FlightKey::forward, true)));
    input.pointer(true);
    REQUIRE(std::holds_alternative<std::monostate>(input.motion({100, 0})));
    const auto before = input.pilot();
    REQUIRE(std::get<Error>(input.key(FlightKey::count, true)) == Error::invalid);
    REQUIRE(std::get<Error>(input.mode(std::bit_cast<Mode>(std::uint8_t{255}))) == Error::invalid);
    REQUIRE(std::get<Error>(input.motion({std::numeric_limits<float>::quiet_NaN(), 0})) ==
            Error::invalid);
    REQUIRE(std::get<Error>(input.wheel(17)) == Error::invalid);
    REQUIRE(input.pilot().thrust == before.thrust);
    REQUIRE(input.pilot().rotation == before.rotation);
    REQUIRE(input.capture());
    input.focus(false);
    REQUIRE(std::holds_alternative<std::monostate>(input.key(FlightKey::forward, true)));
    input.pointer(true);
    REQUIRE(std::holds_alternative<std::monostate>(input.motion({100, 100})));
    REQUIRE(std::holds_alternative<std::monostate>(input.wheel(2)));
    input.focus(true);
    REQUIRE(!input.capture());
    REQUIRE(input.pilot().thrust == std::array<float, 3>{});
    REQUIRE(input.pilot().rotation == std::array<float, 3>{});
    const auto events = input.take_camera();
    REQUIRE(events.orbit.zoom_steps == 0);
    REQUIRE(events.look.yaw_radians == 0);
}

TEST_CASE("a bounded event flood still emits motion admitted by the separate camera rig") {
    FlightInput input;
    auto rig = std::get<CameraRig>(CameraRig::create(3));
    input.pointer(true);
    for (unsigned i = 0; i < 128; ++i) {
        REQUIRE(std::holds_alternative<std::monostate>(input.motion({32768, 32768})));
        REQUIRE(std::holds_alternative<std::monostate>(input.wheel(16)));
    }
    REQUIRE(std::holds_alternative<std::monostate>(rig.orbit(input.take_camera().orbit)));
    REQUIRE(std::holds_alternative<std::monostate>(input.mode(Mode::direct)));
    REQUIRE(std::holds_alternative<std::monostate>(input.key(FlightKey::free_look, true)));
    input.pointer(true);
    for (unsigned i = 0; i < 128; ++i)
        REQUIRE(std::holds_alternative<std::monostate>(input.motion({32768, -32768})));
    REQUIRE(std::holds_alternative<std::monostate>(rig.look(input.take_camera().look)));
    REQUIRE(input.pilot().rotation == std::array<float, 3>{});
}
