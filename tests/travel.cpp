#include "travel/journey.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <stdexcept>

using namespace ngen;

TEST_CASE("a committed path crosses astronomical cells without flattening the origin") {
    constexpr std::int64_t cell = 145'000'000'000;
    constexpr coordinates::Position start{{{cell, .25}, {-cell, -.5}, {cell + 3, 16}}};
    constexpr coordinates::Position destination{
        {{cell + 36, 1.75}, {-cell - 12, -1.5}, {cell + 3, 18}}};
    constexpr coordinates::Position halfway{{{cell + 18, 1}, {-cell - 6, -1}, {cell + 3, 17}}};
    const auto path =
        std::get<travel::Trajectory>(travel::Trajectory::create(start, destination, 8));
    CHECK(path.sample(0) == Result<coordinates::Position>{start});
    CHECK(path.sample(4) == Result<coordinates::Position>{halfway});
    CHECK(path.sample(8) == Result<coordinates::Position>{destination});
    CHECK(std::get<Error>(path.sample(9)) == Error::order);
}

TEST_CASE("warp coordinate bounds are independent of local physics bounds") {
    constexpr auto min = std::numeric_limits<std::int64_t>::min();
    constexpr auto max = std::numeric_limits<std::int64_t>::max();
    constexpr coordinates::Position start{{{min + 128, 0x1p31 - .5}, {max - 128, -.25}, {}}};
    constexpr coordinates::Position end{{{min, -0x1p31}, {max, 1.25}, {}}};
    const auto path = std::get<travel::Trajectory>(travel::Trajectory::create(start, end, 2));
    constexpr coordinates::Position half{{{min + 64, -.25}, {max - 64, .5}, {}}};
    CHECK(path.sample(1) == Result<coordinates::Position>{half});
    auto invalid = end;
    invalid[0].cell = max;
    CHECK(std::get<Error>(travel::Trajectory::create(start, invalid, 2)) == Error::invalid);
    invalid = end;
    invalid[1].offset_metres = 0x1p31;
    CHECK(std::get<Error>(travel::Trajectory::create(start, invalid, 2)) == Error::invalid);
    CHECK(std::get<Error>(travel::Trajectory::create(start, end, 0)) == Error::invalid);
    CHECK(std::get<Error>(travel::Trajectory::create(
              start, end, travel::Trajectory::duration_limit + 1)) == Error::invalid);
}

namespace {
constexpr travel::Route route_fixture() {
    return {{{{145'000'000'000, .25}, {}, {}}},
            {{{145'000'000'036, 1.75}, {}, {}}},
            4,
            17,
            true,
            true,
            true};
}
flight::Snapshot local_flight(const travel::Snapshot &snapshot) {
    if (snapshot.local)
        return *snapshot.local;
    throw std::runtime_error("the journey did not publish a local contact body");
}
} // namespace

TEST_CASE("warp entry checks trusted vessel and route policy") {
    auto route = route_fixture();
    SECTION("incapable hull") { route.jump_capable = false; }
    SECTION("unavailable route") { route.route_allowed = false; }
    SECTION("security denied") { route.security_allowed = false; }
    auto journey = std::get<std::unique_ptr<travel::Journey>>(travel::Journey::create(route, {}));
    const auto initial = journey->snapshot();
    const auto rejected = journey->begin({initial.vessel, initial.tick, 1});
    REQUIRE(std::holds_alternative<Error>(rejected));
    CHECK(std::get<Error>(rejected) == Error::invalid);
    CHECK(journey->snapshot().phase == travel::Phase::local);
}

TEST_CASE("entry cancellation keeps the original body but commitment retires it") {
    const auto route = route_fixture();
    auto journey = std::get<std::unique_ptr<travel::Journey>>(travel::Journey::create(route, {}));
    const auto initial = journey->snapshot();
    const auto body = local_flight(initial).body.id;
    const auto entry =
        std::get<travel::Snapshot>(journey->begin({initial.vessel, initial.tick, 1}));
    REQUIRE(entry.phase == travel::Phase::entry);
    CHECK(local_flight(entry).body.id == body);
    const auto cancelled =
        std::get<travel::Snapshot>(journey->cancel({initial.vessel, initial.tick, 2}, 1));
    REQUIRE(cancelled.phase == travel::Phase::local);
    CHECK(local_flight(cancelled).body.id == body);
    CHECK(cancelled.tick == initial.tick);
    REQUIRE(std::holds_alternative<travel::Snapshot>(
        journey->begin({initial.vessel, initial.tick, 3})));
    const auto committed = std::get<travel::Snapshot>(journey->step());
    REQUIRE(committed.phase == travel::Phase::transit);
    CHECK(committed.tick == Tick{1});
    CHECK_FALSE(committed.local.has_value());
    CHECK(std::get<Error>(journey->cancel({initial.vessel, committed.tick, 4}, 3)) == Error::busy);
    REQUIRE(std::holds_alternative<travel::Snapshot>(journey->step()));
    const auto midway = std::get<travel::Snapshot>(journey->step());
    constexpr coordinates::Position half{{{145'000'000'018, 1}, {}, {}}};
    CHECK(midway.route_position == half);
    CHECK(midway.tick == Tick{3});
    REQUIRE(std::holds_alternative<travel::Snapshot>(journey->step()));
    const auto exit = std::get<travel::Snapshot>(journey->step());
    CHECK(exit.phase == travel::Phase::exit);
    CHECK(exit.route_position == route.destination);
    CHECK(exit.tick == Tick{5});
    CHECK_FALSE(exit.local.has_value());
}

TEST_CASE("only a prepared destination can republish the vessel, exactly once") {
    const auto route = route_fixture();
    auto journey = std::get<std::unique_ptr<travel::Journey>>(travel::Journey::create(route, {}));
    const auto initial = journey->snapshot();
    const auto body = local_flight(initial).body.id;
    const auto vessel = initial.vessel;
    REQUIRE(std::holds_alternative<std::monostate>(
        journey->submit({Tick{1}, 1, vessel, flight::SetMass{1200}})));
    CHECK(std::get<Error>(journey->begin({vessel, Tick{0}, 2})) == Error::busy);
    REQUIRE(local_flight(std::get<travel::Snapshot>(journey->step())).body.mass_kg == 1200);
    REQUIRE(std::holds_alternative<travel::Snapshot>(journey->begin({vessel, Tick{1}, 2})));
    for (unsigned i = 0; i <= route.duration_ticks; ++i)
        REQUIRE(std::holds_alternative<travel::Snapshot>(journey->step()));
    const auto exit = journey->snapshot();
    REQUIRE(exit.phase == travel::Phase::exit);
    REQUIRE(exit.tick == Tick{6});
    REQUIRE_FALSE(exit.local.has_value());
    CHECK(std::get<Error>(journey->arrive({vessel, exit.tick, 3}, 2, nullptr)) == Error::busy);
    const auto waiting = std::get<travel::Snapshot>(journey->step());
    REQUIRE(waiting.phase == travel::Phase::exit);
    CHECK(waiting.route_position == route.destination);
    CHECK_FALSE(waiting.local.has_value());

    const std::array obstacles{physics::Box{{1, 1, 1}, {5, 0, 0}, 1000, false}};
    const travel::CollisionSite ready{route.destination, route.collision_revision, obstacles};
    const travel::Command arrival{vessel, waiting.tick, 3};
    auto stale = ready;
    ++stale.revision;
    CHECK(std::get<Error>(journey->arrive(arrival, 2, &stale)) == Error::stale);
    stale = ready;
    ++stale.origin[0].cell;
    CHECK(std::get<Error>(journey->arrive(arrival, 2, &stale)) == Error::stale);
    CHECK(std::get<Error>(journey->arrive(arrival, 1, &ready)) == Error::stale);
    const std::array occupied{physics::Box{{1, 1, 1}, {3.025, 0, 0}, 1000, false}};
    const travel::CollisionSite blocked{route.destination, route.collision_revision, occupied};
    CHECK(std::get<Error>(journey->arrive(arrival, 2, &blocked)) == Error::busy);
    auto dynamic = obstacles;
    dynamic[0].dynamic = true;
    const travel::CollisionSite malformed{route.destination, route.collision_revision, dynamic};
    CHECK(std::get<Error>(journey->arrive(arrival, 2, &malformed)) == Error::invalid);
    CHECK(journey->snapshot().phase == travel::Phase::exit);
    CHECK(journey->snapshot().tick == waiting.tick);
    CHECK_FALSE(journey->snapshot().local.has_value());
    const auto arrived =
        std::get<travel::Snapshot>(journey->arrive({vessel, waiting.tick, 3}, 2, &ready));
    REQUIRE(arrived.phase == travel::Phase::arrived);
    CHECK(arrived.vessel == vessel);
    CHECK(arrived.tick == Tick{7});
    const auto local = local_flight(arrived);
    CHECK(local.tick == arrived.tick);
    CHECK(local.body.tick == arrived.tick);
    CHECK(local.frame.stamp.tick == arrived.tick);
    CHECK(local.body.frame == local.frame.stamp);
    CHECK(local.frame.origin == route.destination);
    CHECK(local.body.mass_kg == 1200);
    CHECK(local.body.id.world != body.world);
    CHECK(std::get<Error>(journey->arrive({vessel, waiting.tick, 3}, 2, &ready)) == Error::order);
    CHECK(std::get<Error>(journey->arrive({vessel, waiting.tick, 4}, 2, &ready)) == Error::busy);
    CHECK(local_flight(journey->snapshot()).body.id == local.body.id);

    // Exercise destination contacts through the existing flight control path.
    REQUIRE(std::holds_alternative<std::monostate>(
        journey->submit({Tick{8}, 4, vessel, flight::Destination{{20, 0, 0}}})));
    for (unsigned i = 0; i < 240; ++i)
        REQUIRE(std::holds_alternative<travel::Snapshot>(journey->step()));
    const auto at_wall = local_flight(journey->snapshot()).body;
    CHECK(at_wall.id == local.body.id);
    CHECK(at_wall.position_metres[0] > 1.9);
    CHECK(at_wall.position_metres[0] < 2.1);
}
