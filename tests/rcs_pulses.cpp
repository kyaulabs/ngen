#include "flight_demo/preset.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>

using namespace ngen;

namespace {
std::array<double, 3> rotated(std::array<double, 3> v, const std::array<float, 4> &q) {
    const auto cross = [](const auto &a, const auto &b) {
        return std::array<double, 3>{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
                                     a[0] * b[1] - a[1] * b[0]};
    };
    const std::array<double, 3> xyz{q[0], q[1], q[2]};
    const auto first = cross(xyz, v), second = cross(xyz, first);
    for (std::size_t axis = 0; axis < 3; ++axis)
        v[axis] += 2 * (q[3] * first[axis] + second[axis]);
    return v;
}
std::array<std::array<double, 3>, 2> expected_wrench(const flight::ShipDefinition &ship,
                                                     const flight::Snapshot &after,
                                                     const std::array<float, 4> &attitude) {
    if (!ship.rcs)
        throw std::logic_error("physical wrench expectation requires an RCS rig");
    const auto &rig = *ship.rcs;
    std::array<double, 3> force{
        0, 0, -static_cast<double>(ship.maximum_force_newtons[2]) * after.main_drive_level},
        torque{};
    for (std::size_t bank = 0; bank < flight::rcs_bank_count; ++bank) {
        const auto &jet = rig.banks[bank];
        std::array<double, 3> applied{};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            applied[axis] = static_cast<double>(jet.force_newtons[axis]) * after.rcs_firing[bank];
            force[axis] += applied[axis];
        }
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const auto b = (axis + 1) % 3, c = (axis + 2) % 3;
            torque[axis] += static_cast<double>(jet.position_metres[b]) * applied[c] -
                            static_cast<double>(jet.position_metres[c]) * applied[b];
        }
    }
    return {rotated(force, attitude), rotated(torque, attitude)};
}
} // namespace

TEST_CASE("physical RCS admission rejects incomplete or geometrically inconsistent definitions") {
    auto ship = demo::frigate_profile().flight;
    if (!ship.rcs)
        throw std::logic_error("physical admission fixture requires an RCS rig");
    auto &rig = *ship.rcs;
    SECTION("zero thrust bank") { rig.banks[0].force_newtons = {}; }
    SECTION("incorrect finite allocation") { rig.mix[0][0] = 1; }
    SECTION("duplicate forward main drive") { rig.mix[0][5] = .1F; }
    SECTION("nonfinite position") {
        rig.banks[0].position_metres[0] = std::numeric_limits<float>::quiet_NaN();
    }
    SECTION("out of range bank position") { rig.banks[0].position_metres[0] = 1025; }
    SECTION("invalid calibration") { rig.mix[0][0] = -.1F; }
    REQUIRE(std::holds_alternative<Error>(flight::Flight::create(ship)));
}

TEST_CASE("published RCS firings supply the force and moment actually applied to Jolt") {
    const auto ship = demo::frigate_profile().flight;
    REQUIRE(ship.rcs.has_value());
    auto game = std::get<std::unique_ptr<flight::Flight>>(flight::Flight::create(ship));
    const auto initial = game->snapshot();
    flight::Pilot pilot;
    pilot.thrust[0] = 1;
    REQUIRE(std::holds_alternative<std::monostate>(
        game->submit({Tick{1}, 1, initial.entity, flight::SetMode{flight::Mode::direct}})));
    REQUIRE(
        std::holds_alternative<std::monostate>(game->submit({Tick{1}, 2, initial.entity, pilot})));
    const auto after = std::get<flight::Snapshot>(game->step());
    const auto expected = expected_wrench(ship, after, initial.body.rotation_xyzw);
    const auto &force = expected[0], &torque = expected[1];
    for (std::size_t axis = 0; axis < 3; ++axis) {
        REQUIRE(std::abs(after.force_newtons[axis] - force[axis]) <=
                std::max(1., std::abs(force[axis])) * 1e-5);
        REQUIRE(std::abs(after.torque_newton_metres[axis] - torque[axis]) <=
                std::max(1., std::abs(torque[axis])) * 1e-5);
    }
    REQUIRE(after.body.velocity_mps[0] > 0);
}

TEST_CASE("rotating mixed actuation publishes exactly the physical jet and main-drive wrench") {
    auto ship = demo::frigate_profile().flight;
    SECTION("normal flight") {}
    SECTION("speed admission scales actual firings") { ship.maximum_speed_mps = .5F; }
    auto game = std::get<std::unique_ptr<flight::Flight>>(flight::Flight::create(ship));
    const auto initial = game->snapshot();
    flight::Pilot pilot;
    pilot.thrust = {.3F, .2F, -.4F};
    pilot.rotation = {.2F, -.3F, .1F};
    REQUIRE(std::holds_alternative<std::monostate>(
        game->submit({Tick{1}, 1, initial.entity, flight::SetMode{flight::Mode::direct}})));
    REQUIRE(
        std::holds_alternative<std::monostate>(game->submit({Tick{1}, 2, initial.entity, pilot})));
    for (unsigned tick = 0; tick < 180; ++tick) {
        const auto before = game->snapshot();
        const auto after = std::get<flight::Snapshot>(game->step());
        const auto expected = expected_wrench(ship, after, before.body.rotation_xyzw);
        double largest_force = 1, largest_torque = 1, speed = 0;
        for (std::size_t axis = 0; axis < 3; ++axis) {
            largest_force = std::max(largest_force, std::abs(expected[0][axis]));
            largest_torque = std::max(largest_torque, std::abs(expected[1][axis]));
            speed +=
                static_cast<double>(after.body.velocity_mps[axis]) * after.body.velocity_mps[axis];
        }
        REQUIRE(std::sqrt(speed) <= ship.maximum_speed_mps + 1e-4);
        for (std::size_t axis = 0; axis < 3; ++axis) {
            REQUIRE(std::abs(after.force_newtons[axis] - expected[0][axis]) < largest_force * 5e-6);
            REQUIRE(std::abs(after.torque_newton_metres[axis] - expected[1][axis]) <
                    largest_torque * 5e-6);
            const auto dv = after.body.velocity_mps[axis] - before.body.velocity_mps[axis];
            REQUIRE(std::abs(dv - after.force_newtons[axis] / after.body.mass_kg / 60) < 1e-5);
        }
        for (const auto level : after.rcs_firing)
            REQUIRE((std::isfinite(level) && level >= 0 && level <= 1));
    }
}

TEST_CASE(
    "physical coast has no jet impulse and reset discards valve state with owning definitions") {
    auto ship = demo::frigate_profile().flight;
    auto game = std::get<std::unique_ptr<flight::Flight>>(flight::Flight::create(ship));
    if (!ship.rcs)
        throw std::logic_error("physical ownership fixture requires an RCS rig");
    ship.rcs->banks[0].force_newtons = {}; // Cannot change the copied rig/reset.
    auto entity = game->snapshot().entity;
    flight::Pilot pilot;
    pilot.thrust[0] = .1F;
    const auto start = [&] {
        REQUIRE(std::holds_alternative<std::monostate>(
            game->submit({Tick{1}, 1, entity, flight::SetMode{flight::Mode::direct}})));
        REQUIRE(std::holds_alternative<std::monostate>(game->submit({Tick{1}, 2, entity, pilot})));
    };
    start();
    const auto first = std::get<flight::Snapshot>(game->step());
    REQUIRE(first.rcs_firing != flight::RcsLevels{});
    unsigned quiet = 0;
    for (unsigned tick = 1; tick < 180; ++tick) {
        const auto before = game->snapshot();
        const auto after = std::get<flight::Snapshot>(game->step());
        if (after.rcs_firing == flight::RcsLevels{} && after.main_drive_level == 0) {
            REQUIRE(after.force_newtons == std::array<float, 3>{});
            REQUIRE(after.torque_newton_metres == std::array<float, 3>{});
            REQUIRE(after.body.velocity_mps == before.body.velocity_mps);
            ++quiet;
        }
    }
    REQUIRE(quiet > 0);
    const auto reset = std::get<flight::Snapshot>(game->reset());
    REQUIRE(reset.rcs_firing == flight::RcsLevels{});
    REQUIRE(reset.main_drive_level == 0);
    REQUIRE(reset.rcs_demand_level == 0);
    entity = reset.entity;
    start();
    const auto restarted = std::get<flight::Snapshot>(game->step());
    REQUIRE(restarted.rcs_firing == first.rcs_firing);
    REQUIRE(restarted.force_newtons == first.force_newtons);
    REQUIRE(restarted.torque_newton_metres == first.torque_newton_metres);
}

TEST_CASE(
    "saturated lateral authority sustains physical thrust despite small bank calibration weights") {
    const auto ship = demo::frigate_profile().flight;
    auto game = std::get<std::unique_ptr<flight::Flight>>(flight::Flight::create(ship));
    const auto entity = game->snapshot().entity;
    flight::Pilot pilot;
    pilot.thrust[0] = 1;
    REQUIRE(std::holds_alternative<std::monostate>(
        game->submit({Tick{1}, 1, entity, flight::SetMode{flight::Mode::direct}})));
    REQUIRE(std::holds_alternative<std::monostate>(game->submit({Tick{1}, 2, entity, pilot})));
    for (unsigned tick = 0; tick < 24; ++tick) {
        const auto state = std::get<flight::Snapshot>(game->step());
        REQUIRE(state.force_newtons[0] >= ship.maximum_force_newtons[0] * .9999F);
        REQUIRE(state.rcs_firing != flight::RcsLevels{});
    }
}

TEST_CASE("the accepted forward main drive remains separate and coasts without hidden RCS thrust") {
    const auto ship = demo::frigate_profile().flight;
    auto game = std::get<std::unique_ptr<flight::Flight>>(flight::Flight::create(ship));
    const auto entity = game->snapshot().entity;
    flight::Pilot pilot;
    pilot.thrust[2] = -1;
    REQUIRE(std::holds_alternative<std::monostate>(
        game->submit({Tick{1}, 1, entity, flight::SetMode{flight::Mode::direct}})));
    REQUIRE(std::holds_alternative<std::monostate>(game->submit({Tick{1}, 2, entity, pilot})));
    for (unsigned tick = 0; tick < 60; ++tick) {
        const auto state = std::get<flight::Snapshot>(game->step());
        REQUIRE(state.main_drive_level == 1);
        REQUIRE(state.rcs_firing == flight::RcsLevels{});
        REQUIRE(state.force_newtons == std::array<float, 3>{0, 0, -ship.maximum_force_newtons[2]});
        REQUIRE(state.torque_newton_metres == std::array<float, 3>{});
    }
    const auto before = game->snapshot();
    REQUIRE(std::holds_alternative<std::monostate>(
        game->submit({Tick{61}, 3, entity, flight::Pilot{}})));
    const auto coast = std::get<flight::Snapshot>(game->step());
    REQUIRE(coast.main_drive_level == 0);
    REQUIRE(coast.rcs_firing == flight::RcsLevels{});
    REQUIRE(coast.force_newtons == std::array<float, 3>{});
    REQUIRE(coast.body.velocity_mps == before.body.velocity_mps);
}

TEST_CASE("small steady RCS corrections produce puffs with coordinated quiet intervals") {
    flight::RcsPulses pulses;
    demo::RcsLevels demand{};
    demand[0] = .01F;
    demand[5] = .006F;
    demand[11] = .004F;
    unsigned firing = 0, quiet = 0;
    for (unsigned tick = 0; tick < 180; ++tick) {
        const auto result = pulses.step(demand);
        REQUIRE(std::holds_alternative<demo::RcsLevels>(result));
        const auto levels = std::get<demo::RcsLevels>(result);
        if (levels == demo::RcsLevels{})
            ++quiet;
        else {
            REQUIRE(levels == demand); // No unbalanced partial moment pair or new brightness.
            ++firing;
        }
    }
    REQUIRE(firing > 0);
    REQUIRE(quiet > firing);
}

TEST_CASE("opposing RCS demand fires immediately without inheriting the old set's off debt") {
    flight::RcsPulses pulses;
    demo::RcsLevels turn{}, stop{};
    turn[0] = .01F;
    turn[5] = .006F;
    stop[3] = .01F;
    stop[8] = .006F;
    for (unsigned tick = 0; tick < 6; ++tick)
        REQUIRE(std::get<demo::RcsLevels>(pulses.step(turn)) == turn);
    REQUIRE(std::get<demo::RcsLevels>(pulses.step(turn)) == demo::RcsLevels{});
    REQUIRE(std::get<demo::RcsLevels>(pulses.step(stop)) == stop);
}

TEST_CASE("RCS idle hysteresis suppresses residual chatter and cancels stale burns") {
    flight::RcsPulses pulses;
    demo::RcsLevels demand{};
    demand[2] = .00015F;
    for (unsigned tick = 0; tick < 120; ++tick)
        REQUIRE(std::get<demo::RcsLevels>(pulses.step(demand)) == demo::RcsLevels{});
    demand[2] = .00025F;
    REQUIRE(std::get<demo::RcsLevels>(pulses.step(demand)) == demand);
    demand[2] = .00015F;
    REQUIRE(std::get<demo::RcsLevels>(pulses.step(demand)) == demand);
    demand[2] = .00009F;
    REQUIRE(std::get<demo::RcsLevels>(pulses.step(demand)) == demo::RcsLevels{});
    demand[2] = .00015F;
    for (unsigned tick = 0; tick < 120; ++tick)
        REQUIRE(std::get<demo::RcsLevels>(pulses.step(demand)) == demo::RcsLevels{});
}

TEST_CASE("correction puffs respect minimum burn and off times without random flicker") {
    flight::RcsPulses pulses;
    demo::RcsLevels demand{};
    demand[4] = .01F;
    bool previous = true;
    unsigned length = 0, firing = 0;
    for (unsigned tick = 0; tick < 600; ++tick) {
        const auto levels = std::get<demo::RcsLevels>(pulses.step(demand));
        const bool burning = levels != demo::RcsLevels{};
        if (tick == 0)
            REQUIRE(burning);
        if (burning == previous)
            ++length;
        else {
            REQUIRE(length >= (previous ? 6U : 3U));
            length = 1;
            previous = burning;
        }
        firing += burning ? 1U : 0U;
    }
    // Existing perceptual response gives about 31.6% visual duty at 1% demand.
    REQUIRE(firing >= 180);
    REQUIRE(firing <= 198);
}

TEST_CASE("only saturated RCS demand stays on continuously and release extinguishes it") {
    flight::RcsPulses pulses;
    demo::RcsLevels demand{};
    demand[1] = 1;
    demand[8] = .27F;
    for (unsigned tick = 0; tick < 600; ++tick)
        REQUIRE(std::get<demo::RcsLevels>(pulses.step(demand)) == demand);
    demand[1] = .8F;
    unsigned quiet = 0;
    for (unsigned tick = 0; tick < 600; ++tick)
        quiet += std::get<demo::RcsLevels>(pulses.step(demand)) == demo::RcsLevels{} ? 1U : 0U;
    REQUIRE(quiet > 0);
    REQUIRE(std::get<demo::RcsLevels>(pulses.step({})) == demo::RcsLevels{});
    REQUIRE(pulses.levels() == demo::RcsLevels{});
}

TEST_CASE("a saturated correction preserves its minimum puff while easing into partial demand") {
    flight::RcsPulses pulses;
    demo::RcsLevels demand{};
    demand[1] = 1;
    REQUIRE(std::get<demo::RcsLevels>(pulses.step(demand)) == demand);
    demand[1] = .01F;
    for (unsigned tick = 1; tick < 6; ++tick)
        REQUIRE(std::get<demo::RcsLevels>(pulses.step(demand)) == demand);
}

TEST_CASE("invalid pulse demands leave the complete next firing sequence unchanged") {
    flight::RcsPulses pulses;
    demo::RcsLevels demand{};
    demand[0] = .02F;
    for (unsigned tick = 0; tick < 11; ++tick)
        REQUIRE(std::holds_alternative<demo::RcsLevels>(pulses.step(demand)));
    auto expected = pulses;
    const auto before = pulses.levels();
    auto invalid = demand;
    SECTION("negative") { invalid[15] = -.01F; }
    SECTION("above unit") { invalid[15] = 1.01F; }
    SECTION("NaN") { invalid[15] = std::numeric_limits<float>::quiet_NaN(); }
    SECTION("infinity") { invalid[15] = std::numeric_limits<float>::infinity(); }
    REQUIRE(std::holds_alternative<Error>(pulses.step(invalid)));
    REQUIRE(pulses.levels() == before);
    for (unsigned tick = 0; tick < 180; ++tick)
        REQUIRE(std::get<demo::RcsLevels>(pulses.step(demand)) ==
                std::get<demo::RcsLevels>(expected.step(demand)));
}

TEST_CASE("pulse reset drops stale credit and copied outputs own their values") {
    flight::RcsPulses pulses, fresh;
    demo::RcsLevels demand{};
    demand[5] = .04F;
    const auto saved = std::get<demo::RcsLevels>(pulses.step(demand));
    for (unsigned tick = 0; tick < 17; ++tick)
        REQUIRE(std::holds_alternative<demo::RcsLevels>(pulses.step(demand)));
    pulses.reset();
    REQUIRE(pulses.levels() == demo::RcsLevels{});
    REQUIRE(saved == demand);
    for (unsigned tick = 0; tick < 180; ++tick)
        REQUIRE(std::get<demo::RcsLevels>(pulses.step(demand)) ==
                std::get<demo::RcsLevels>(fresh.step(demand)));
}

TEST_CASE("render reads at different cadences never advance the fixed-tick pulse sequence") {
    flight::RcsPulses fast, slow;
    demo::RcsLevels demand{};
    demand[3] = .03F;
    for (unsigned tick = 0; tick < 180; ++tick) {
        const auto expected = std::get<demo::RcsLevels>(fast.step(demand));
        REQUIRE(std::get<demo::RcsLevels>(slow.step(demand)) == expected);
        // 120 Hz reads versus 30 Hz reads around the same 60 Hz tick sequence.
        REQUIRE(fast.levels() == expected);
        REQUIRE(fast.levels() == expected);
        if (tick % 2 == 0)
            REQUIRE(slow.levels() == expected);
    }
}

TEST_CASE("actual attitude arrest wakes a coordinated RCS puff after the pilot releases") {
    const auto profile = demo::frigate_profile();
    for (std::size_t axis = 0; axis < 3; ++axis) {
        auto game =
            std::get<std::unique_ptr<flight::Flight>>(flight::Flight::create(profile.flight));
        const auto entity = game->snapshot().entity;
        flight::Pilot pilot;
        pilot.rotation[axis] = 1;
        REQUIRE(std::holds_alternative<std::monostate>(
            game->submit({Tick{1}, 1, entity, flight::SetMode{flight::Mode::direct}})));
        REQUIRE(std::holds_alternative<std::monostate>(game->submit({Tick{1}, 2, entity, pilot})));
        demo::RcsLevels starting{};
        for (unsigned tick = 0; tick < 180; ++tick) {
            const auto after = std::get<flight::Snapshot>(game->step());
            const auto &visible = after.rcs_firing;
            if (tick == 0)
                starting = visible;
        }
        REQUIRE(game->snapshot().rcs_firing == demo::RcsLevels{});
        const auto before = game->snapshot();
        REQUIRE(before.body.angular_velocity_rps[axis] > .35F);
        REQUIRE(std::holds_alternative<std::monostate>(
            game->submit({Tick{181}, 3, entity, flight::Pilot{}})));
        const auto after = std::get<flight::Snapshot>(game->step());
        REQUIRE(after.torque_newton_metres[axis] < 0);
        const auto &visible = after.rcs_firing;
        REQUIRE(std::count_if(visible.begin(), visible.end(), [](float v) { return v > 0; }) >= 2);
        for (std::size_t bank = 0; bank < demo::rcs_bank_count; ++bank)
            REQUIRE((starting[bank] <= 1e-6F || visible[bank] <= 1e-6F));
        for (const auto position : after.body.position_metres)
            REQUIRE(std::abs(position) < 1e-4);
    }
}
