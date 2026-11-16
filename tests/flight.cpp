#include "flight/flight.hpp"
#include <bit>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <thread>
using namespace ngen;
using namespace ngen::flight;
namespace {
std::array<double, 3> body_force(const std::array<float, 4> &q, const std::array<float, 3> &v) {
    const double x = -q[0], y = -q[1], z = -q[2], w = q[3];
    const double tx = 2 * (y * v[2] - z * v[1]);
    const double ty = 2 * (z * v[0] - x * v[2]);
    const double tz = 2 * (x * v[1] - y * v[0]);
    return {v[0] + w * tx + y * tz - z * ty, v[1] + w * ty + z * tx - x * tz,
            v[2] + w * tz + x * ty - y * tx};
}
} // namespace

TEST_CASE("direct pilot thrust reaches the fixed-step physical hull at the next boundary") {
    const ShipDefinition definition;
    auto created = Flight::create(definition);
    REQUIRE(std::holds_alternative<std::unique_ptr<Flight>>(created));
    auto flight = std::get<std::unique_ptr<Flight>>(std::move(created));
    const auto before = flight->snapshot();
    REQUIRE(before.mode == Mode::tactical);
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 1, before.entity, SetMode{Mode::direct}})));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 2, before.entity, Pilot{{1, 0, 0}}})));
    REQUIRE(flight->snapshot().tick == Tick{0});
    for (unsigned tick = 0; tick < 60; ++tick)
        REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
    const auto after = flight->snapshot();
    REQUIRE(after.mode == Mode::direct);
    REQUIRE(after.tick == Tick{60});
    REQUIRE(after.body.velocity_mps[0] == Catch::Approx(6).margin(1e-4));
    REQUIRE(after.force_newtons[0] == 6000);
    REQUIRE(before.body.velocity_mps[0] == 0);
}

TEST_CASE("direct input release coasts and the explicit brake uses bounded opposing thrust") {
    const ShipDefinition definition;
    auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(definition));
    const auto entity = flight->snapshot().entity;
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 1, entity, SetMode{Mode::direct}})));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 2, entity, Pilot{{1, 0, 0}}})));
    for (unsigned tick = 0; tick < 60; ++tick)
        REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
    REQUIRE(std::holds_alternative<std::monostate>(flight->submit({Tick{61}, 3, entity, Pilot{}})));
    for (unsigned tick = 0; tick < 60; ++tick)
        REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
    REQUIRE(flight->snapshot().body.velocity_mps[0] == Catch::Approx(6).margin(1e-4));
    REQUIRE(flight->snapshot().force_newtons[0] == 0);
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{121}, 4, entity, Pilot{{}, {}, true}})));
    const auto first = std::get<Snapshot>(flight->step());
    REQUIRE(first.force_newtons[0] < 0);
    REQUIRE(first.body.velocity_mps[0] > 5.8F);
    for (unsigned tick = 0; tick < 119; ++tick) {
        const auto state = std::get<Snapshot>(flight->step());
        REQUIRE(std::abs(state.force_newtons[0]) <= definition.maximum_force_newtons[0]);
    }
    REQUIRE(std::abs(flight->snapshot().body.velocity_mps[0]) < .1F);
}

TEST_CASE("direct attitude intent and release act through bounded torque stabilization") {
    const ShipDefinition definition;
    auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(definition));
    const auto entity = flight->snapshot().entity;
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 1, entity, SetMode{Mode::direct}})));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 2, entity, Pilot{{}, {1, 0, 0}}})));
    for (unsigned tick = 0; tick < 120; ++tick) {
        const auto state = std::get<Snapshot>(flight->step());
        REQUIRE(std::abs(state.torque_newton_metres[0]) <=
                definition.maximum_torque_newton_metres[0]);
    }
    const auto rotating = flight->snapshot();
    REQUIRE(rotating.body.angular_velocity_rps[0] == Catch::Approx(.5).margin(.001));
    REQUIRE(rotating.body.rotation_xyzw[0] > .3F);
    REQUIRE(
        std::holds_alternative<std::monostate>(flight->submit({Tick{121}, 3, entity, Pilot{}})));
    const auto braking = std::get<Snapshot>(flight->step());
    REQUIRE(braking.torque_newton_metres[0] < 0);
    REQUIRE(braking.body.angular_velocity_rps[0] > .4F);
    for (unsigned tick = 0; tick < 119; ++tick)
        REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
    REQUIRE(std::abs(flight->snapshot().body.angular_velocity_rps[0]) < .001F);
    REQUIRE(flight->snapshot().body.position_metres == std::array<double, 3>{});
}

TEST_CASE("the direct speed limit limits applied force instead of resetting velocity") {
    ShipDefinition definition;
    definition.maximum_speed_mps = 10;
    auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(definition));
    const auto entity = flight->snapshot().entity;
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 1, entity, SetMode{Mode::direct}})));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 2, entity, Pilot{{1, 0, 0}}})));
    for (unsigned tick = 0; tick < 600; ++tick) {
        const auto state = std::get<Snapshot>(flight->step());
        REQUIRE(state.body.velocity_mps[0] <= 10.0001F);
        REQUIRE(std::abs(state.force_newtons[0]) <= definition.maximum_force_newtons[0]);
    }
    const auto limited = flight->snapshot();
    REQUIRE(limited.body.velocity_mps[0] == Catch::Approx(10).margin(.001));
    REQUIRE(limited.force_newtons[0] == Catch::Approx(0).margin(.01));
    REQUIRE(
        std::holds_alternative<std::monostate>(flight->submit({Tick{601}, 3, entity, Pilot{}})));
    REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
    REQUIRE(flight->snapshot().body.velocity_mps[0] ==
            Catch::Approx(limited.body.velocity_mps[0]).margin(1e-5));
}

TEST_CASE("a tactical destination converges through the same bounded physical force model") {
    ShipDefinition definition;
    definition.maximum_speed_mps = 20;
    auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(definition));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 1, flight->snapshot().entity, Destination{{100, 0, 0}}})));
    for (unsigned tick = 0; tick < 1800; ++tick) {
        const auto before = flight->snapshot();
        const auto state = std::get<Snapshot>(flight->step());
        REQUIRE(state.mode == Mode::tactical);
        // Limits are body-local; the published actuation is world-space.
        const auto local = body_force(before.body.rotation_xyzw, state.force_newtons);
        for (std::size_t axis = 0; axis < local.size(); ++axis)
            REQUIRE(std::abs(local[axis]) <= definition.maximum_force_newtons[axis] + .01);
        REQUIRE(std::abs(state.body.velocity_mps[0]) <= 20.0001F);
    }
    const auto arrived = flight->snapshot();
    REQUIRE(std::abs(arrived.body.position_metres[0] - 100) < .1);
    REQUIRE(std::abs(arrived.body.velocity_mps[0]) < .1F);
    REQUIRE(std::abs(arrived.body.position_metres[1]) < .01);
    REQUIRE(std::abs(arrived.body.position_metres[2]) < .01);
}

TEST_CASE(
    "tactical steering turns the hull toward its destination using the shared attitude model") {
    ShipDefinition definition;
    definition.maximum_speed_mps = 20;
    auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(definition));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 1, flight->snapshot().entity, Destination{{200, 0, 0}}})));
    for (unsigned tick = 0; tick < 600; ++tick)
        REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
    const auto state = flight->snapshot();
    const auto &q = state.body.rotation_xyzw;
    const auto forward_x = -2 * (q[0] * q[2] + q[3] * q[1]);
    const auto forward_y = -2 * (q[1] * q[2] - q[3] * q[0]);
    REQUIRE(forward_x > .95F);
    REQUIRE(std::abs(forward_y) < .05F);
    REQUIRE(state.body.position_metres[0] > 100);
}

TEST_CASE(
    "clockwise and counterclockwise orbit intents settle within the declared flight envelope") {
    for (const auto clockwise : {false, true}) {
        ShipDefinition definition;
        definition.hull.position_metres = {100, 0, 0};
        definition.maximum_speed_mps = 20;
        auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(definition));
        REQUIRE(std::holds_alternative<std::monostate>(flight->submit(
            {Tick{1}, 1, flight->snapshot().entity, Orbit{{0, 0, 0}, 100, 10, clockwise}})));
        for (unsigned tick = 0; tick < 7200; ++tick) {
            const auto before = flight->snapshot();
            const auto state = std::get<Snapshot>(flight->step());
            const auto local = body_force(before.body.rotation_xyzw, state.force_newtons);
            for (std::size_t axis = 0; axis < local.size(); ++axis)
                REQUIRE(std::abs(local[axis]) <= definition.maximum_force_newtons[axis] + .01);
            if (tick >= 1800) {
                const auto &p = state.body.position_metres;
                REQUIRE(std::abs(std::hypot(p[0], p[2]) - 100) < 1);
                REQUIRE(std::abs(p[1]) < .1);
                const auto &v = state.body.velocity_mps;
                REQUIRE(std::abs(std::hypot(v[0], v[2]) - 10) < .2F);
            }
        }
        const auto state = flight->snapshot();
        const auto &p = state.body.position_metres;
        const auto &v = state.body.velocity_mps;
        const auto angular_y = p[2] * v[0] - p[0] * v[2];
        REQUIRE((angular_y < 0) == clockwise);
    }
}

TEST_CASE("co-moving payload mass changes acceleration without a pose or velocity impulse") {
    auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(ShipDefinition{}));
    const auto entity = flight->snapshot().entity;
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 1, entity, SetMode{Mode::direct}})));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 2, entity, Pilot{{1, 0, 0}}})));
    for (unsigned tick = 0; tick < 60; ++tick)
        REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
    const auto before = flight->snapshot();
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{61}, 3, entity, SetMass{2000}})));
    REQUIRE(std::holds_alternative<std::monostate>(flight->submit({Tick{61}, 4, entity, Pilot{}})));
    const auto changed = std::get<Snapshot>(flight->step());
    REQUIRE(changed.body.mass_kg == 2000);
    REQUIRE(changed.body.velocity_mps == before.body.velocity_mps);
    REQUIRE(changed.body.rotation_xyzw == before.body.rotation_xyzw);
    REQUIRE(changed.body.position_metres[0] ==
            Catch::Approx(before.body.position_metres[0] + before.body.velocity_mps[0] / 60)
                .margin(1e-5));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{62}, 5, entity, Pilot{{1, 0, 0}}})));
    for (unsigned tick = 0; tick < 60; ++tick)
        REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
    REQUIRE(flight->snapshot().body.velocity_mps[0] ==
            Catch::Approx(before.body.velocity_mps[0] + 3).margin(1e-3));
}

TEST_CASE("a jointly infeasible orbit and mass batch changes neither native nor published state") {
    auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(ShipDefinition{}));
    const auto before = flight->snapshot();
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 1, before.entity, Orbit{{0, 0, 0}, 100, 10, false}})));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 2, before.entity, SetMass{10000}})));
    const auto rejected = flight->step();
    REQUIRE(std::holds_alternative<Error>(rejected));
    REQUIRE(std::get<Error>(rejected) == Error::invalid);
    REQUIRE(flight->snapshot().tick == before.tick);
    REQUIRE(flight->snapshot().body.mass_kg == before.body.mass_kg);
    REQUIRE(flight->snapshot().body.position_metres == before.body.position_metres);
    REQUIRE(flight->snapshot().body.velocity_mps == before.body.velocity_mps);
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 3, before.entity, SetMode{Mode::direct}})));
    const auto recovered = std::get<Snapshot>(flight->step());
    REQUIRE(recovered.tick == Tick{1});
    REQUIRE(recovered.body.mass_kg == 1000);
    REQUIRE(recovered.navigation.goal == Goal::coast);
    REQUIRE(recovered.body.position_metres == before.body.position_metres);
}

TEST_CASE("foreign-thread intents and stepping fail without poisoning the owner") {
    auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(ShipDefinition{}));
    const Command command{Tick{1}, 1, flight->snapshot().entity, SetMode{Mode::direct}};
    Result<std::monostate> submitted{std::monostate{}};
    Result<Snapshot> advanced{Error::invalid}, reset{Error::invalid};
    std::thread foreign([&] {
        submitted = flight->submit(command);
        advanced = flight->step();
        reset = flight->reset();
    });
    foreign.join();
    REQUIRE(std::holds_alternative<Error>(submitted));
    REQUIRE(std::get<Error>(submitted) == Error::order);
    REQUIRE(std::get<Error>(advanced) == Error::order);
    REQUIRE(std::get<Error>(reset) == Error::order);
    REQUIRE(flight->snapshot().tick == Tick{0});
    REQUIRE(std::holds_alternative<std::monostate>(flight->submit(command)));
    REQUIRE(std::get<Snapshot>(flight->step()).tick == Tick{1});
}

TEST_CASE("a speed limiter cannot demand unavailable lateral braking from an anisotropic hull") {
    ShipDefinition definition;
    definition.maximum_speed_mps = 10;
    definition.maximum_force_newtons = {6000, .01F, 6000};
    auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(definition));
    const auto entity = flight->snapshot().entity;
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 1, entity, SetMode{Mode::direct}})));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 2, entity, Pilot{{1, 0, 0}}})));
    for (unsigned tick = 0; tick < 200; ++tick)
        REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{201}, 3, entity, Pilot{{}, {0, 0, 1}}})));
    for (unsigned tick = 0; tick < 150; ++tick)
        REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{351}, 4, entity, Pilot{{1, 0, 0}}})));
    for (unsigned tick = 0; tick < 60; ++tick) {
        const auto before = flight->snapshot();
        const auto state = std::get<Snapshot>(flight->step());
        const auto &v = state.body.velocity_mps;
        REQUIRE(std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) <= 10.0001F);
        const auto local = body_force(before.body.rotation_xyzw, state.force_newtons);
        REQUIRE(std::abs(local[1]) <= .010001);
    }
}

TEST_CASE("reset replaces the physical scene and invalidates queued and stale-generation intents") {
    ShipDefinition definition;
    auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(definition));
    const auto entity = flight->snapshot().entity;
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 1, entity, SetMode{Mode::direct}})));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 2, entity, Pilot{{1, 0, 0}, {0, .5F, 0}}})));
    for (unsigned tick = 0; tick < 60; ++tick)
        REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
    const auto saved = flight->snapshot();
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{61}, 3, entity, SetMass{2000}})));
    const auto reset = flight->reset();
    REQUIRE(std::holds_alternative<Snapshot>(reset));
    const auto initial = std::get<Snapshot>(reset);
    REQUIRE(initial.tick == Tick{0});
    REQUIRE(initial.mode == Mode::tactical);
    REQUIRE(initial.navigation.goal == Goal::coast);
    REQUIRE(initial.body.position_metres == definition.hull.position_metres);
    REQUIRE(initial.body.velocity_mps == std::array<float, 3>{});
    REQUIRE(initial.body.angular_velocity_rps == std::array<float, 3>{});
    REQUIRE(initial.body.mass_kg == 1000);
    REQUIRE(initial.entity != entity);
    REQUIRE(saved.tick == Tick{60});
    REQUIRE(saved.body.velocity_mps[0] > 5);
    const auto stale = flight->submit({Tick{1}, 1, entity, SetMode{Mode::direct}});
    REQUIRE(std::get<Error>(stale) == Error::stale);
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 1, initial.entity, SetMode{Mode::direct}})));
    REQUIRE(std::get<Snapshot>(flight->step()).body.mass_kg == 1000);
    for (unsigned iteration = 0; iteration < 8; ++iteration)
        REQUIRE(std::holds_alternative<Snapshot>(flight->reset()));
}

TEST_CASE("invalid and out-of-order commands neither consume sequence nor borrow caller storage") {
    auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(ShipDefinition{}));
    const auto entity = flight->snapshot().entity;
    for (const auto &intent :
         {Intent{Pilot{{std::numeric_limits<float>::quiet_NaN(), 0, 0}}},
          Intent{Pilot{{1.01F, 0, 0}}}, Intent{SetMode{std::bit_cast<Mode>(std::uint8_t{255})}},
          Intent{Destination{{0, std::numeric_limits<double>::infinity(), 0}}},
          Intent{Orbit{{}, 0, 10, false}}, Intent{SetMass{0}}}) {
        const auto rejected = flight->submit({Tick{1}, 1, entity, intent});
        REQUIRE(std::get<Error>(rejected) == Error::invalid);
    }
    REQUIRE(std::get<Error>(flight->submit({Tick{2}, 1, entity, Pilot{}})) == Error::order);
    REQUIRE(std::get<Error>(flight->submit({Tick{0}, 1, entity, Pilot{}})) == Error::order);
    REQUIRE(std::get<Error>(flight->submit({Tick{1}, 0, entity, Pilot{}})) == Error::order);
    const Entity stale{entity.slot, entity.generation + 1};
    REQUIRE(std::get<Error>(flight->submit({Tick{1}, 1, stale, Pilot{}})) == Error::stale);
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 1, entity, SetMode{Mode::direct}})));
    REQUIRE(std::get<Error>(flight->submit({Tick{1}, 1, entity, Pilot{}})) == Error::order);
    Command source{Tick{1}, 2, entity, Pilot{{1, 0, 0}}};
    REQUIRE(std::holds_alternative<std::monostate>(flight->submit(source)));
    source.intent = Pilot{};
    const auto published = std::get<Snapshot>(flight->step());
    REQUIRE(published.force_newtons[0] == Catch::Approx(6000));
    REQUIRE(published.body.velocity_mps[0] == Catch::Approx(.1F));
}

TEST_CASE("the next-tick queue rejects its sixty-fifth command without replacing accepted intent") {
    auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(ShipDefinition{}));
    const auto entity = flight->snapshot().entity;
    for (std::uint64_t i = 1; i <= Flight::command_capacity; ++i)
        REQUIRE(std::holds_alternative<std::monostate>(
            flight->submit({Tick{1}, i, entity, SetMode{i % 2 ? Mode::tactical : Mode::direct}})));
    REQUIRE(std::get<Error>(flight->submit({Tick{1}, 65, entity, SetMode{Mode::tactical}})) ==
            Error::capacity);
    REQUIRE(flight->snapshot().tick == Tick{0});
    REQUIRE(std::get<Snapshot>(flight->step()).mode == Mode::direct);
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{2}, 65, entity, SetMode{Mode::tactical}})));
    REQUIRE(std::get<Snapshot>(flight->step()).mode == Mode::tactical);
}

TEST_CASE("manual control cancels old autopilot instead of restarting it on tactical return") {
    for (const auto &goal :
         {Intent{Destination{{100, 0, 0}}}, Intent{Orbit{{0, 0, -100}, 100, 10, false}}}) {
        auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(ShipDefinition{}));
        const auto entity = flight->snapshot().entity;
        REQUIRE(std::holds_alternative<std::monostate>(flight->submit({Tick{1}, 1, entity, goal})));
        for (unsigned tick = 0; tick < 60; ++tick)
            REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
        const auto before = flight->snapshot();
        REQUIRE(std::holds_alternative<std::monostate>(
            flight->submit({Tick{61}, 2, entity, SetMode{Mode::direct}})));
        const auto direct = std::get<Snapshot>(flight->step());
        REQUIRE(direct.navigation.goal == Goal::coast);
        REQUIRE(direct.force_newtons == std::array<float, 3>{});
        REQUIRE(direct.body.velocity_mps == before.body.velocity_mps);
        REQUIRE(std::holds_alternative<std::monostate>(
            flight->submit({Tick{62}, 3, entity, Pilot{{0, 1, 0}, {0, 1, 0}}})));
        for (unsigned tick = 0; tick < 60; ++tick)
            REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
        const auto manual = flight->snapshot();
        REQUIRE(std::holds_alternative<std::monostate>(
            flight->submit({Tick{122}, 4, entity, Pilot{}})));
        REQUIRE(std::holds_alternative<std::monostate>(
            flight->submit({Tick{122}, 5, entity, SetMode{Mode::tactical}})));
        for (unsigned tick = 0; tick < 120; ++tick) {
            const auto state = std::get<Snapshot>(flight->step());
            REQUIRE(state.mode == Mode::tactical);
            REQUIRE(state.navigation.goal == Goal::coast);
            REQUIRE(state.force_newtons == std::array<float, 3>{});
            REQUIRE(state.body.velocity_mps == manual.body.velocity_mps);
            REQUIRE(state.entity == entity);
        }
        REQUIRE(std::holds_alternative<std::monostate>(
            flight->submit({Tick{242}, 6, entity, Destination{{50, 0, -50}}})));
        const auto commanded = std::get<Snapshot>(flight->step());
        REQUIRE(commanded.navigation.goal == Goal::destination);
        REQUIRE(commanded.force_newtons != std::array<float, 3>{});
    }
}

TEST_CASE(
    "mode changes during thrust and spin preserve pose and momentum instead of snapping state") {
    auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(ShipDefinition{}));
    const auto entity = flight->snapshot().entity;
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 1, entity, SetMode{Mode::direct}})));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{1}, 2, entity, Pilot{{1, 0, 0}, {0, 1, 0}}})));
    for (unsigned tick = 0; tick < 120; ++tick)
        REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
    const auto before = flight->snapshot();
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{121}, 3, entity, SetMode{Mode::tactical}})));
    const auto tactical = std::get<Snapshot>(flight->step());
    REQUIRE(tactical.body.velocity_mps == before.body.velocity_mps);
    REQUIRE(tactical.body.rotation_xyzw != std::array<float, 4>{0, 0, 0, 1});
    REQUIRE(tactical.body.angular_velocity_rps[1] > .4F);
    REQUIRE(tactical.force_newtons == std::array<float, 3>{});
    REQUIRE(tactical.entity == before.entity);
    for (std::size_t axis = 0; axis < 3; ++axis)
        REQUIRE(
            tactical.body.position_metres[axis] ==
            Catch::Approx(before.body.position_metres[axis] + tactical.body.velocity_mps[axis] / 60)
                .margin(1e-5));
    REQUIRE(std::holds_alternative<std::monostate>(
        flight->submit({Tick{122}, 4, entity, SetMode{Mode::direct}})));
    const auto direct = std::get<Snapshot>(flight->step());
    REQUIRE(direct.entity == before.entity);
    REQUIRE(direct.body.angular_velocity_rps[1] > .4F);
    for (std::size_t axis = 0; axis < 3; ++axis)
        REQUIRE(std::abs(direct.body.velocity_mps[axis] - tactical.body.velocity_mps[axis]) < .18F);
}

TEST_CASE(
    "fixed tick command replay is independent of presentation cadence in the same native build") {
    const auto replay = [](const auto &cadence) {
        auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(ShipDefinition{}));
        std::array<Snapshot, 600> states;
        std::size_t tick = 0, frame = 0;
        std::uint64_t sequence = 0;
        while (tick < states.size()) {
            const auto count = cadence[frame++ % cadence.size()];
            for (unsigned i = 0; i < count && tick < states.size(); ++i) {
                const auto send = [&](Intent intent) {
                    REQUIRE(std::holds_alternative<std::monostate>(flight->submit(
                        {Tick{tick + 1}, ++sequence, flight->snapshot().entity, intent})));
                };
                switch (tick + 1) {
                case 1:
                    send(SetMode{Mode::direct});
                    send(Pilot{{1, 0, 0}});
                    break;
                case 61:
                    send(Pilot{});
                    break;
                case 121:
                    send(Pilot{{}, {}, true});
                    break;
                case 181:
                    send(Destination{{100, 0, 0}});
                    break;
                case 301:
                    send(SetMode{Mode::direct});
                    send(Pilot{{0, 0, -1}, {0, .5F, 0}});
                    break;
                case 361:
                    send(Pilot{});
                    break;
                case 421:
                    send(SetMass{2000});
                    break;
                case 481:
                    send(Orbit{{}, 100, 10, false});
                    break;
                default:
                    break;
                }
                states[tick++] = std::get<Snapshot>(flight->step());
            }
        }
        return states;
    };
    const auto fixed = replay(std::array<unsigned, 1>{1});
    const auto variable = replay(std::array<unsigned, 5>{0, 2, 0, 1, 3});
    for (std::size_t i = 0; i < fixed.size(); ++i) {
        REQUIRE(fixed[i].tick == variable[i].tick);
        REQUIRE(fixed[i].mode == variable[i].mode);
        REQUIRE(fixed[i].body.position_metres == variable[i].body.position_metres);
        REQUIRE(fixed[i].body.rotation_xyzw == variable[i].body.rotation_xyzw);
        REQUIRE(fixed[i].body.velocity_mps == variable[i].body.velocity_mps);
        REQUIRE(fixed[i].body.angular_velocity_rps == variable[i].body.angular_velocity_rps);
        REQUIRE(fixed[i].body.mass_kg == variable[i].body.mass_kg);
        REQUIRE(fixed[i].force_newtons == variable[i].force_newtons);
        REQUIRE(fixed[i].torque_newton_metres == variable[i].torque_newton_metres);
    }
}

TEST_CASE(
    "declared static obstacles share the physical flight domain and survive an owning reset") {
    std::array obstacles{physics::Box{{1, 4, 4}, {8, 0, 0}, 1000, false}};
    auto flight = std::get<std::unique_ptr<Flight>>(Flight::create(ShipDefinition{}, obstacles));
    obstacles[0].position_metres = {100, 0, 0};
    for (unsigned run = 0; run < 2; ++run) {
        const auto entity = flight->snapshot().entity;
        REQUIRE(std::holds_alternative<std::monostate>(
            flight->submit({Tick{1}, 1, entity, SetMode{Mode::direct}})));
        REQUIRE(std::holds_alternative<std::monostate>(
            flight->submit({Tick{1}, 2, entity, Pilot{{1, 0, 0}}})));
        for (unsigned tick = 0; tick < 180; ++tick)
            REQUIRE(std::holds_alternative<Snapshot>(flight->step()));
        const auto stopped = flight->snapshot();
        REQUIRE(stopped.body.position_metres[0] <= 5.25);
        REQUIRE(stopped.body.position_metres[0] >= 4.9);
        REQUIRE(std::abs(stopped.body.velocity_mps[0]) < .3F);
        REQUIRE(std::holds_alternative<Snapshot>(flight->reset()));
    }
}

TEST_CASE("the contact preset bounds capacity and rejects invalid partial construction without "
          "publication") {
    std::array<physics::Box, physics::World::body_capacity> obstacles{};
    for (std::size_t i = 0; i < obstacles.size(); ++i) {
        obstacles[i].dynamic = false;
        obstacles[i].half_extent_metres = {.5F, .5F, .5F};
        obstacles[i].position_metres = {20 + static_cast<double>(i) * 3, 0, 0};
    }
    REQUIRE(std::get<Error>(Flight::create(ShipDefinition{}, obstacles)) == Error::capacity);
    const auto admitted = std::span(obstacles).first(obstacles.size() - 1);
    REQUIRE(std::holds_alternative<std::unique_ptr<Flight>>(
        Flight::create(ShipDefinition{}, admitted)));
    obstacles[0].dynamic = true;
    REQUIRE(std::get<Error>(Flight::create(ShipDefinition{}, admitted)) == Error::invalid);
    obstacles[0].dynamic = false;
    obstacles[obstacles.size() - 2].half_extent_metres[0] = 0;
    REQUIRE(std::get<Error>(Flight::create(ShipDefinition{}, admitted)) == Error::invalid);
    REQUIRE(std::holds_alternative<std::unique_ptr<Flight>>(Flight::create(ShipDefinition{})));
}
