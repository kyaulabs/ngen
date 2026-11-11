#include "physics/world.hpp"
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <thread>
using namespace ngen;
using namespace ngen::physics;

TEST_CASE("a fixed-step local hull accelerates from force divided by its declared mass") {
    auto created = World::create();
    REQUIRE(std::holds_alternative<std::unique_ptr<World>>(created));
    auto world = std::get<std::unique_ptr<World>>(std::move(created));
    const auto admitted = world->add_box(Box{{2, 1, 1}, {}, 1000});
    REQUIRE(std::holds_alternative<BodyId>(admitted));
    const auto id = std::get<BodyId>(admitted);
    const auto before = std::get<BodyState>(world->state(id));
    for (unsigned tick = 0; tick < World::ticks_per_second; ++tick) {
        REQUIRE(std::holds_alternative<std::monostate>(world->apply(id, {6000, 0, 0})));
        REQUIRE(std::holds_alternative<Tick>(world->step()));
    }
    const auto after = std::get<BodyState>(world->state(id));
    REQUIRE(after.tick == Tick{60});
    REQUIRE(after.velocity_mps[0] == Catch::Approx(6).margin(1e-4));
    REQUIRE(after.velocity_mps[1] == 0);
    REQUIRE(after.velocity_mps[2] == 0);
    REQUIRE(after.position_metres[0] == Catch::Approx(3.05).margin(1e-4));
    REQUIRE(before.position_metres[0] == 0);
    REQUIRE(before.velocity_mps[0] == 0);
}

TEST_CASE("published local positions retain sub-millimetre offsets through the physics adapter") {
    auto world = std::get<std::unique_ptr<World>>(World::create());
    const std::array<double, 3> position{4096.001, -4096.003, .00025};
    const auto id = std::get<BodyId>(world->add_box(Box{{1, 1, 1}, position, 1000}));
    for (unsigned tick = 0; tick < 60; ++tick)
        REQUIRE(std::holds_alternative<Tick>(world->step()));
    const auto state = std::get<BodyState>(world->state(id));
    for (std::size_t axis = 0; axis < position.size(); ++axis)
        REQUIRE(std::abs(state.position_metres[axis] - position[axis]) < 1e-8);
}

TEST_CASE("changing co-moving hull mass preserves velocity and changes subsequent acceleration") {
    auto world = std::get<std::unique_ptr<World>>(World::create());
    const auto id = std::get<BodyId>(world->add_box(Box{{2, 1, 1}, {}, 1000}));
    for (unsigned tick = 0; tick < 60; ++tick) {
        REQUIRE(std::holds_alternative<std::monostate>(world->apply(id, {6000, 0, 0})));
        REQUIRE(std::holds_alternative<Tick>(world->step()));
    }
    const auto before = std::get<BodyState>(world->state(id));
    REQUIRE(std::holds_alternative<std::monostate>(world->set_mass(id, 2000)));
    REQUIRE(std::get<BodyState>(world->state(id)).velocity_mps == before.velocity_mps);
    for (unsigned tick = 0; tick < 60; ++tick) {
        REQUIRE(std::holds_alternative<std::monostate>(world->apply(id, {6000, 0, 0})));
        REQUIRE(std::holds_alternative<Tick>(world->step()));
    }
    const auto after = std::get<BodyState>(world->state(id));
    REQUIRE(after.velocity_mps[0] == Catch::Approx(9).margin(1e-4));
    REQUIRE(after.mass_kg == 2000);
    REQUIRE(before.mass_kg == 1000);
}

TEST_CASE("released thrust coasts and an opposing physical force brakes without a velocity reset") {
    auto world = std::get<std::unique_ptr<World>>(World::create());
    const auto id = std::get<BodyId>(world->add_box(Box{{2, 1, 1}, {}, 1000}));
    for (unsigned tick = 0; tick < 60; ++tick) {
        REQUIRE(std::holds_alternative<std::monostate>(world->apply(id, {6000, 0, 0})));
        REQUIRE(std::holds_alternative<Tick>(world->step()));
    }
    const auto released = std::get<BodyState>(world->state(id));
    for (unsigned tick = 0; tick < 600; ++tick)
        REQUIRE(std::holds_alternative<Tick>(world->step()));
    const auto coasted = std::get<BodyState>(world->state(id));
    REQUIRE(coasted.velocity_mps[0] == Catch::Approx(released.velocity_mps[0]).margin(1e-5));
    REQUIRE(coasted.position_metres[0] - released.position_metres[0] ==
            Catch::Approx(60).margin(1e-3));
    for (unsigned tick = 0; tick < 60; ++tick) {
        REQUIRE(std::holds_alternative<std::monostate>(world->apply(id, {-6000, 0, 0})));
        REQUIRE(std::holds_alternative<Tick>(world->step()));
    }
    const auto braked = std::get<BodyState>(world->state(id));
    REQUIRE(braked.velocity_mps[0] == Catch::Approx(0).margin(1e-4));
    REQUIRE(braked.position_metres[0] > coasted.position_metres[0] + 2.9);
}

TEST_CASE("a bounded local static obstacle stops a driven rigid hull") {
    auto world = std::get<std::unique_ptr<World>>(World::create());
    const auto wall = world->add_box(Box{{.5F, 10, 10}, {}, 1000, false});
    REQUIRE(std::holds_alternative<BodyId>(wall));
    const auto id = std::get<BodyId>(world->add_box(Box{{.5F, .5F, .5F}, {-5, 0, 0}, 1}));
    for (unsigned tick = 0; tick < 240; ++tick) {
        REQUIRE(std::holds_alternative<std::monostate>(world->apply(id, {6, 0, 0})));
        REQUIRE(std::holds_alternative<Tick>(world->step()));
    }
    const auto state = std::get<BodyState>(world->state(id));
    REQUIRE(state.position_metres[0] <= -.95);
    REQUIRE(std::abs(state.velocity_mps[0]) < .01F);
    REQUIRE(std::get<BodyState>(world->state(std::get<BodyId>(wall))).position_metres[0] == 0);
}

TEST_CASE("a hull rotates from torque and shape inertia without translational acceleration") {
    auto world = std::get<std::unique_ptr<World>>(World::create());
    const auto id = std::get<BodyId>(world->add_box(Box{{2, 1, 1}, {}, 1000}));
    for (unsigned tick = 0; tick < 60; ++tick) {
        REQUIRE(std::holds_alternative<std::monostate>(world->apply(id, {}, {2000.0F / 3, 0, 0})));
        REQUIRE(std::holds_alternative<Tick>(world->step()));
    }
    const auto state = std::get<BodyState>(world->state(id));
    REQUIRE(state.angular_velocity_rps[0] == Catch::Approx(1).margin(1e-4));
    REQUIRE(state.angular_velocity_rps[1] == Catch::Approx(0).margin(1e-6));
    REQUIRE(state.angular_velocity_rps[2] == Catch::Approx(0).margin(1e-6));
    REQUIRE(state.velocity_mps == std::array<float, 3>{});
    REQUIRE(state.rotation_xyzw[0] > .2F);
    REQUIRE(state.rotation_xyzw[3] < 1);
}

TEST_CASE(
    "physics rejects nonfinite inputs foreign handles and off-owner access without mutation") {
    auto world = std::get<std::unique_ptr<World>>(World::create());
    auto other = std::get<std::unique_ptr<World>>(World::create());
    const auto id = std::get<BodyId>(world->add_box({}));
    const auto foreign = std::get<BodyId>(other->add_box({}));
    const auto before = std::get<BodyState>(world->state(id));
    for (const auto invalid : {0.0, -1.0, std::numeric_limits<double>::infinity(),
                               std::numeric_limits<double>::quiet_NaN()}) {
        REQUIRE(std::get<Error>(world->set_mass(id, invalid)) == Error::invalid);
        auto box = Box{};
        box.mass_kg = invalid;
        REQUIRE(std::get<Error>(world->add_box(box)) == Error::invalid);
    }
    const auto nan = std::numeric_limits<float>::quiet_NaN();
    REQUIRE(std::get<Error>(world->apply(id, {nan, 0, 0})) == Error::invalid);
    REQUIRE(std::get<Error>(world->apply(id, {}, {0, nan, 0})) == Error::invalid);
    REQUIRE(std::get<Error>(world->apply(foreign, {1, 0, 0})) == Error::stale);
    auto stale = id;
    ++stale.generation;
    REQUIRE(std::get<Error>(world->state(stale)) == Error::stale);
    bool off_owner_rejected = false;
    std::thread worker([&] {
        const auto state = world->state(id);
        off_owner_rejected =
            std::holds_alternative<Error>(state) && std::get<Error>(state) == Error::order;
    });
    worker.join();
    REQUIRE(off_owner_rejected);
    const auto after = std::get<BodyState>(world->state(id));
    REQUIRE(after.tick == before.tick);
    REQUIRE(after.position_metres == before.position_metres);
    REQUIRE(after.velocity_mps == before.velocity_mps);
    REQUIRE(after.mass_kg == before.mass_kg);
    REQUIRE(std::holds_alternative<std::monostate>(world->apply(id, {6000, 0, 0})));
    REQUIRE(std::holds_alternative<Tick>(world->step()));
    REQUIRE(std::get<BodyState>(world->state(id)).velocity_mps[0] ==
            Catch::Approx(.1).margin(1e-6));
}

TEST_CASE("local body capacity rejects excess admission without replacing existing bodies") {
    auto world = std::get<std::unique_ptr<World>>(World::create());
    std::array<BodyId, World::body_capacity> ids{};
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const auto admitted =
            world->add_box(Box{{1, 1, 1}, {4.0 * static_cast<double>(i), 0, 0}, 1000, false});
        REQUIRE(std::holds_alternative<BodyId>(admitted));
        ids[i] = std::get<BodyId>(admitted);
    }
    REQUIRE(std::get<Error>(world->add_box({})) == Error::capacity);
    for (std::size_t i = 0; i < ids.size(); ++i) {
        const auto state = std::get<BodyState>(world->state(ids[i]));
        REQUIRE(state.position_metres[0] == 4.0 * static_cast<double>(i));
        REQUIRE(std::get<Error>(world->apply(ids[i], {1, 0, 0})) == Error::invalid);
    }
    REQUIRE(std::holds_alternative<Tick>(world->step()));
}

TEST_CASE("unsupported impulses are rejected before the native velocity clamps can lose force") {
    auto world = std::get<std::unique_ptr<World>>(World::create());
    const auto id = std::get<BodyId>(world->add_box(Box{{2, 1, 1}, {}, 1000}));
    const auto force = world->apply(id, {4e7F, 0, 0});
    REQUIRE(std::holds_alternative<Error>(force));
    REQUIRE(std::get<Error>(force) == Error::invalid);
    const auto torque = world->apply(id, {}, {1e12F, 0, 0});
    REQUIRE(std::holds_alternative<Error>(torque));
    REQUIRE(std::get<Error>(torque) == Error::invalid);
    REQUIRE(std::holds_alternative<std::monostate>(world->apply(id, {6000, 0, 0})));
    REQUIRE(std::holds_alternative<Tick>(world->step()));
    const auto state = std::get<BodyState>(world->state(id));
    REQUIRE(state.velocity_mps[0] == Catch::Approx(.1).margin(1e-6));
    REQUIRE(state.angular_velocity_rps == std::array<float, 3>{});
}

TEST_CASE("a mass change cannot make already accepted force exceed the native motion envelope") {
    auto world = std::get<std::unique_ptr<World>>(World::create());
    const auto id = std::get<BodyId>(world->add_box(Box{{2, 1, 1}, {}, 1000}));
    REQUIRE(std::holds_alternative<std::monostate>(world->apply(id, {60000, 0, 0})));
    const auto changed = world->set_mass(id, 1);
    REQUIRE(std::holds_alternative<Error>(changed));
    REQUIRE(std::get<Error>(changed) == Error::invalid);
    REQUIRE(std::get<BodyState>(world->state(id)).mass_kg == 1000);
    REQUIRE(std::holds_alternative<Tick>(world->step()));
    REQUIRE(std::get<BodyState>(world->state(id)).velocity_mps[0] == Catch::Approx(1).margin(1e-5));
}
