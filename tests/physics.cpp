#include "physics/world.hpp"
#include <algorithm>
#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstdio>
#include <limits>
#include <thread>
using namespace ngen;
using namespace ngen::physics;

TEST_CASE("origin transfer moves all live bodies together without consuming a tick", "[rebase]") {
    auto world = std::get<std::unique_ptr<World>>(World::create());
    const auto moving = std::get<BodyId>(world->add_box({}));
    const auto fixed = std::get<BodyId>(world->add_box(Box{{1, 1, 1}, {10, 0, 0}, 1000, false}));
    const auto old_frame = std::get<coordinates::Frame>(world->frame());
    REQUIRE(std::holds_alternative<std::monostate>(world->apply(moving, {6000, 0, 0})));
    const auto changed = world->rebase(old_frame.stamp, {{{0, 128}, {0, -64}, {0, 0}}});
    REQUIRE(std::holds_alternative<coordinates::Frame>(changed));
    const auto &stamp = std::get<coordinates::Frame>(changed).stamp;
    CHECK(stamp == coordinates::FrameStamp{moving.world, old_frame.stamp.epoch + 1, {0}});
    CHECK(std::get<BodyState>(world->state(moving)).position_metres ==
          std::array{-128.0, 64.0, 0.0});
    CHECK(std::get<BodyState>(world->state(fixed)).position_metres ==
          std::array{-118.0, 64.0, 0.0});
    CHECK(std::get<BodyState>(world->state(fixed)).frame == stamp);
    REQUIRE(world->step() == Result<Tick>{Tick{1}});
    CHECK(std::get<BodyState>(world->state(moving)).velocity_mps[0] ==
          Catch::Approx(0.1).margin(1e-6));
}

TEST_CASE("two capped hull sections reach the end of a one kilometre rigid body", "[rebase]") {
    auto world = std::get<std::unique_ptr<World>>(World::create());
    const auto wall = std::get<BodyId>(world->add_box(Box{{1, 60, 60}, {}, 1000, false}));
    const auto long_box = world->add_box(Box{{500, 50, 50}, {-500.99, 0, 0}});
    REQUIRE(long_box == Result<BodyId>{Error::invalid});
    const auto hull =
        std::get<BodyId>(world->add_two_section_hull(Box{{250, 50, 50}, {-500.99, 0, 0}}));
    REQUIRE(std::holds_alternative<Tick>(world->step()));
    CHECK(world->in_contact(hull, wall) == Result<bool>{true});
    CHECK(std::get<BodyState>(world->state(hull)).mass_kg == 1000);
}

TEST_CASE("native center tether remains active after translating its domain", "[rebase]") {
    auto world = std::get<std::unique_ptr<World>>(World::create());
    const auto lhs = std::get<BodyId>(world->add_box({}));
    const auto rhs = std::get<BodyId>(world->add_box(Box{{1, 1, 1}, {10, 0, 0}}));
    REQUIRE(std::holds_alternative<std::monostate>(world->constrain_distance(lhs, rhs, 10)));
    const auto before = std::get<coordinates::Frame>(world->frame());
    REQUIRE(std::holds_alternative<coordinates::Frame>(
        world->rebase(before.stamp, {{{0, 128}, {}, {}}})));
    for (unsigned tick = 0; tick < 60; ++tick) {
        REQUIRE(std::holds_alternative<std::monostate>(world->apply(rhs, {6000, 0, 0})));
        REQUIRE(std::holds_alternative<Tick>(world->step()));
    }
    const auto left = std::get<BodyState>(world->state(lhs));
    const auto right = std::get<BodyState>(world->state(rhs));
    CHECK(left.velocity_mps[0] > 2.5F);
    CHECK(right.position_metres[0] - left.position_metres[0] == Catch::Approx(10).margin(.002));
}

TEST_CASE("rejected origin transfers are atomic and leave snapshots stamped", "[rebase]") {
    auto world = std::get<std::unique_ptr<World>>(World::create());
    const auto first = std::get<BodyId>(world->add_box({}));
    const auto last = std::get<BodyId>(world->add_box(Box{{1, 1, 1}, {8191, 0, 0}, 1000, false}));
    const auto before = std::get<coordinates::Frame>(world->frame());
    const auto first_before = std::get<BodyState>(world->state(first));
    const auto last_before = std::get<BodyState>(world->state(last));
    const coordinates::Position outside{{{0, -2}, {}, {}}};
    auto error = [](const Result<coordinates::Frame> &result) {
        REQUIRE(std::holds_alternative<Error>(result));
        return std::get<Error>(result);
    };
    CHECK(error(world->rebase(before.stamp, outside)) == Error::invalid);
    CHECK(error(world->rebase(before.stamp, before.origin)) == Error::invalid);
    CHECK(error(world->rebase(before.stamp, {{{0, 0x1p31}, {}, {}}})) == Error::invalid);
    const auto after = std::get<coordinates::Frame>(world->frame());
    CHECK(after.stamp == before.stamp);
    CHECK(after.origin == before.origin);
    CHECK(std::get<BodyState>(world->state(first)).position_metres == first_before.position_metres);
    CHECK(std::get<BodyState>(world->state(last)).position_metres == last_before.position_metres);
    REQUIRE(world->step() == Result<Tick>{Tick{1}});
    CHECK(error(world->rebase(before.stamp, {{{0, 1}, {}, {}}})) == Error::stale);
    const auto current = std::get<coordinates::Frame>(world->frame());
    bool off_owner_rejected = false;
    std::thread worker([&] {
        const auto result = world->rebase(current.stamp, {{{0, 1}, {}, {}}});
        const auto *failure = std::get_if<Error>(&result);
        off_owner_rejected = failure && *failure == Error::order;
    });
    worker.join();
    REQUIRE(off_owner_rejected);
    REQUIRE(std::holds_alternative<coordinates::Frame>(
        world->rebase(current.stamp, {{{0, 1}, {}, {}}})));
    CHECK(error(world->rebase(current.stamp, {{{0, 2}, {}, {}}})) == Error::stale);
    CHECK(first_before.frame == before.stamp);
}

namespace {
constexpr std::int64_t distant_cell = 145'000'000'000;
constexpr coordinates::Position dock_origin{{{distant_cell, 0x1p31 - 2048}, {}, {}}};
constexpr coordinates::Position shifted_origin{{{distant_cell + 1, -0x1p31 + 2048}, {}, {}}};
constexpr double rebase_tolerance = .002;
constexpr unsigned transfer_count = 10'000;
struct DockingFixture {
    std::unique_ptr<World> world;
    BodyId hull, follower, dock;
};

DockingFixture approach(float length) {
    auto world = std::get<std::unique_ptr<World>>(World::create(dock_origin));
    const auto half = length / 2;
    const auto head = -static_cast<double>(half) - 3;
    const auto hull = std::get<BodyId>(
        length == 1000 ? world->add_two_section_hull(Box{{250, 50, 50}, {head, 0, 0}})
                       : world->add_box(Box{{half, half, half}, {head, 0, 0}}));
    const auto rest = half + 10;
    const auto follower = std::get<BodyId>(world->add_box(Box{{5, 5, 5}, {head - rest, 0, 0}}));
    const auto dock = std::get<BodyId>(world->add_box(Box{{1, 60, 60}, {}, 1000, false}));
    REQUIRE(
        std::holds_alternative<std::monostate>(world->constrain_distance(follower, hull, rest)));
    return {std::move(world), hull, follower, dock};
}

void push_and_step(const DockingFixture &fixture, unsigned tick) {
    REQUIRE(
        std::holds_alternative<std::monostate>(fixture.world->apply(fixture.hull, {6000, 0, 0})));
    REQUIRE(fixture.world->step() == Result<Tick>{Tick{tick}});
}

double separation(const BodyState &a, const BodyState &b) {
    return std::hypot(a.position_metres[0] - b.position_metres[0],
                      a.position_metres[1] - b.position_metres[1],
                      a.position_metres[2] - b.position_metres[2]);
}

double position_difference(const BodyState &base, const BodyState &moved, double offset) {
    return std::hypot(base.position_metres[0] - (moved.position_metres[0] + offset),
                      base.position_metres[1] - moved.position_metres[1],
                      base.position_metres[2] - moved.position_metres[2]);
}

double speed_difference(const BodyState &base, const BodyState &moved) {
    return std::hypot(static_cast<double>(base.velocity_mps[0]) - moved.velocity_mps[0],
                      static_cast<double>(base.velocity_mps[1]) - moved.velocity_mps[1],
                      static_cast<double>(base.velocity_mps[2]) - moved.velocity_mps[2]);
}
} // namespace

TEST_CASE("a local domain starts at a canonical split origin without moving any body", "[rebase]") {
    const auto noncanonical = World::create({{{distant_cell, 0x1p31}, {}, {}}});
    REQUIRE(std::holds_alternative<Error>(noncanonical));
    CHECK(std::get<Error>(noncanonical) == Error::invalid);
    auto world = std::get<std::unique_ptr<World>>(World::create(dock_origin));
    CHECK(std::get<coordinates::Frame>(world->frame()).origin == dock_origin);
    const auto id = std::get<BodyId>(world->add_box({}));
    CHECK(std::get<BodyState>(world->state(id)).position_metres == std::array<double, 3>{});
}

TEST_CASE("ten thousand live transfers keep contacts constraints and world trajectories",
          "[rebase]") {
    for (const auto length : {10.0F, 100.0F, 1000.0F}) {
        const auto baseline = approach(length);
        const auto shifting = approach(length);
        const auto initial = std::get<coordinates::Frame>(shifting.world->frame());
        auto stamp = initial.stamp;
        unsigned touching = 0;
        double position_error = 0, velocity_error = 0, length_error = 0;
        for (unsigned i = 1; i <= transfer_count; ++i) {
            const bool across_cell = i % 2 != 0;
            const double offset = across_cell ? 4096.0 : 0.0;
            const auto translated =
                shifting.world->rebase(stamp, across_cell ? shifted_origin : dock_origin);
            REQUIRE(std::holds_alternative<coordinates::Frame>(translated));
            const auto &moved = std::get<coordinates::Frame>(translated);
            REQUIRE(moved.stamp.tick == stamp.tick);
            REQUIRE(moved.stamp.epoch == initial.stamp.epoch + i);
            push_and_step(baseline, i);
            push_and_step(shifting, i);
            const auto ref_hull = std::get<BodyState>(baseline.world->state(baseline.hull));
            const auto ref_follower = std::get<BodyState>(baseline.world->state(baseline.follower));
            const auto hull = std::get<BodyState>(shifting.world->state(shifting.hull));
            const auto follower = std::get<BodyState>(shifting.world->state(shifting.follower));
            const auto dock = std::get<BodyState>(shifting.world->state(shifting.dock));
            stamp = hull.frame;
            REQUIRE(stamp ==
                    coordinates::FrameStamp{initial.stamp.id, initial.stamp.epoch + i, {i}});
            REQUIRE(follower.frame == stamp);
            REQUIRE(dock.frame == stamp);
            REQUIRE(dock.position_metres == std::array{-offset, 0.0, 0.0});
            const auto contact =
                std::get<bool>(baseline.world->in_contact(baseline.hull, baseline.dock));
            REQUIRE(shifting.world->in_contact(shifting.hull, shifting.dock) ==
                    Result<bool>{contact});
            if (contact)
                ++touching;
            const auto position = std::max(position_difference(ref_hull, hull, offset),
                                           position_difference(ref_follower, follower, offset));
            const auto speed = std::max(speed_difference(ref_hull, hull),
                                        speed_difference(ref_follower, follower));
            const auto linkage =
                std::abs(separation(hull, follower) - separation(ref_hull, ref_follower));
            CAPTURE(length, i, position, speed, linkage);
            REQUIRE(std::isfinite(position));
            REQUIRE(std::isfinite(speed));
            REQUIRE(std::isfinite(linkage));
            REQUIRE(position <= rebase_tolerance);
            REQUIRE(speed <= rebase_tolerance);
            REQUIRE(linkage <= rebase_tolerance);
            position_error = std::max(position_error, position);
            velocity_error = std::max(velocity_error, speed);
            length_error = std::max(length_error, linkage);
        }
        CHECK(touching > transfer_count / 2);
        CHECK(touching < transfer_count);
        const auto final = std::get<BodyState>(shifting.world->state(shifting.hull));
        CHECK(std::abs(final.velocity_mps[0]) < .002F);
        std::printf("M4-B %.0f m: %u transfers, %u contact ticks; max %.9g m position, "
                    "%.9g m/s velocity, %.9g m tether difference\n",
                    static_cast<double>(length), transfer_count, touching, position_error,
                    velocity_error, length_error);
    }
}

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

TEST_CASE(
    "invalid box extents fail admission before shape construction without consuming capacity") {
    auto world = std::get<std::unique_ptr<World>>(World::create());
    for (std::size_t axis = 0; axis < 3; ++axis)
        for (const auto invalid : {0.0F, -.1F, 257.0F, std::numeric_limits<float>::infinity(),
                                   std::numeric_limits<float>::quiet_NaN()}) {
            auto description = Box{};
            description.half_extent_metres[axis] = invalid;
            const auto result = world->add_box(description);
            REQUIRE(std::holds_alternative<Error>(result));
            REQUIRE(std::get<Error>(result) == Error::invalid);
        }
    REQUIRE(std::holds_alternative<BodyId>(world->add_box({})));
    REQUIRE(std::holds_alternative<Tick>(world->step()));
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
