#include "flight_demo/rcs.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace ngen;

TEST_CASE("small steady RCS corrections produce puffs with coordinated quiet intervals") {
    demo::RcsPulses pulses;
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

TEST_CASE("RCS idle hysteresis suppresses residual chatter and cancels stale burns") {
    demo::RcsPulses pulses;
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

TEST_CASE("correction puffs respect visible minimum burn and off times without random flicker") {
    demo::RcsPulses pulses;
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
    demo::RcsPulses pulses;
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
    demo::RcsPulses pulses;
    demo::RcsLevels demand{};
    demand[1] = 1;
    REQUIRE(std::get<demo::RcsLevels>(pulses.step(demand)) == demand);
    demand[1] = .01F;
    for (unsigned tick = 1; tick < 6; ++tick)
        REQUIRE(std::get<demo::RcsLevels>(pulses.step(demand)) == demand);
}

TEST_CASE("invalid pulse demands leave the complete next firing sequence unchanged") {
    demo::RcsPulses pulses;
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
    REQUIRE(std::holds_alternative<render::Failure>(pulses.step(invalid)));
    REQUIRE(pulses.levels() == before);
    for (unsigned tick = 0; tick < 180; ++tick)
        REQUIRE(std::get<demo::RcsLevels>(pulses.step(demand)) ==
                std::get<demo::RcsLevels>(expected.step(demand)));
}

TEST_CASE("pulse reset drops stale credit and copied outputs own their values") {
    demo::RcsPulses pulses, fresh;
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
    demo::RcsPulses fast, slow;
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
        demo::RcsPulses pulses;
        demo::RcsLevels starting{};
        for (unsigned tick = 0; tick < 180; ++tick) {
            const auto before = game->snapshot();
            const auto after = std::get<flight::Snapshot>(game->step());
            const auto demand = std::get<demo::RcsLevels>(
                demo::rcs_levels(profile, before.body.rotation_xyzw, after.force_newtons,
                                 after.torque_newton_metres));
            const auto visible = std::get<demo::RcsLevels>(pulses.step(demand));
            if (tick == 0)
                starting = visible;
        }
        REQUIRE(pulses.levels() == demo::RcsLevels{});
        const auto before = game->snapshot();
        REQUIRE(before.body.angular_velocity_rps[axis] > .35F);
        REQUIRE(std::holds_alternative<std::monostate>(
            game->submit({Tick{181}, 3, entity, flight::Pilot{}})));
        const auto after = std::get<flight::Snapshot>(game->step());
        REQUIRE(after.torque_newton_metres[axis] < 0);
        const auto demand = std::get<demo::RcsLevels>(demo::rcs_levels(
            profile, before.body.rotation_xyzw, after.force_newtons, after.torque_newton_metres));
        const auto visible = std::get<demo::RcsLevels>(pulses.step(demand));
        REQUIRE(visible == demand);
        REQUIRE(std::count_if(visible.begin(), visible.end(), [](float v) { return v > 0; }) >= 2);
        for (std::size_t bank = 0; bank < demo::rcs_bank_count; ++bank)
            REQUIRE((starting[bank] <= 1e-6F || visible[bank] <= 1e-6F));
        REQUIRE(after.body.position_metres == std::array<double, 3>{});
    }
}
