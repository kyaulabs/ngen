#include "combat/arena.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <type_traits>

using namespace ngen::combat;

static_assert(!std::is_copy_constructible_v<Arena>);
static_assert(!std::is_move_constructible_v<Arena>);

TEST_CASE("a targeted shot must have a valid visible target within range", "[combat]") {
    const TargetedShot attack{Ship::player, Ship::rival, Region::hull};
    auto clear = Arena::create({.separation_metres = 40});
    auto distant = Arena::create({.separation_metres = 41});
    auto covered = Arena::create({.cover = true});
    REQUIRE((clear && distant && covered));
    using Outcome = std::variant<Impact, Reject>;
    CHECK(clear->fire({Ship::none, Ship::rival, Region::hull}) == Outcome{Reject::invalid_shooter});
    CHECK(clear->fire({Ship::player, Ship::none, Region::hull}) == Outcome{Reject::invalid_target});
    CHECK(clear->fire({Ship::player, Ship::player, Region::hull}) ==
          Outcome{Reject::invalid_target});
    CHECK(distant->fire(attack) == Outcome{Reject::out_of_range});
    CHECK(covered->fire(attack) == Outcome{Reject::blocked});
    CHECK(distant->snapshot() == Snapshot{});
    CHECK(covered->snapshot() == Snapshot{});
    CHECK(clear->fire(attack) == Outcome{Impact{Hit::ship, Ship::rival, Region::hull, 20}});
    CHECK(clear->snapshot() == Snapshot{Health{}, Health{80, 20, 20}});
}

TEST_CASE("projectiles sweep between endpoints and report each nearest impact", "[combat]") {
    SECTION("the segment hits even when both endpoints are outside the target") {
        auto arena = Arena::create({});
        REQUIRE(arena);
        REQUIRE_FALSE(arena->launch({Ship::player, {48, 0, 0}}));
        REQUIRE_FALSE(arena->launch({Ship::rival, {-24, 0, 0}}));
        REQUIRE_FALSE(arena->launch({Ship::player, {48, 8, 0}}));
        CHECK(arena->snapshot() == Snapshot{});
        const auto report = arena->advance();
        REQUIRE(report.size == 3);
        CHECK(report.impacts[0] == Impact{Hit::ship, Ship::rival, Region::hull, 35});
        CHECK(report.impacts[1] == Impact{Hit::ship, Ship::player, Region::hull, 35});
        CHECK(report.impacts[2] == Impact{});
        CHECK(arena->snapshot() == Snapshot{Health{65, 20, 20}, Health{65, 20, 20}});
        CHECK(arena->advance().size == 0);
    }
    SECTION("an interior mount cannot override an earlier hull impact") {
        auto arena = Arena::create({});
        REQUIRE(arena);
        REQUIRE_FALSE(arena->launch({Ship::player, {48, 2, 0}}));
        const auto report = arena->advance();
        REQUIRE(report.size == 1);
        CHECK(report.impacts[0] == Impact{Hit::ship, Ship::rival, Region::hull, 35});
        CHECK(arena->snapshot() == Snapshot{Health{}, Health{65, 20, 20}});
    }
    SECTION("nearer cover intercepts before the target") {
        auto arena = Arena::create({.cover = true});
        REQUIRE(arena);
        REQUIRE_FALSE(arena->launch({Ship::player, {48, 0, 0}}));
        const auto report = arena->advance();
        REQUIRE(report.size == 1);
        CHECK(report.impacts[0] == Impact{Hit::cover});
        CHECK(arena->snapshot() == Snapshot{});
    }
}

TEST_CASE("component impacts disable only their fitted weapon", "[combat]") {
    auto arena = Arena::create({});
    REQUIRE(arena);
    using Outcome = std::variant<Impact, Reject>;
    CHECK(arena->fire({Ship::player, Ship::rival, Region::targeted_mount}) ==
          Outcome{Impact{Hit::ship, Ship::rival, Region::targeted_mount, 20}});
    CHECK(arena->fire({Ship::rival, Ship::player, Region::hull}) == Outcome{Reject::disabled});
    REQUIRE_FALSE(arena->launch({Ship::rival, {-24, 0, 0}}));
    REQUIRE_FALSE(arena->launch({Ship::player, {48, -3, 0}}));
    const auto report = arena->advance();
    REQUIRE(report.size == 2);
    CHECK(report.impacts[0] == Impact{Hit::ship, Ship::player, Region::hull, 35});
    CHECK(report.impacts[1] == Impact{Hit::ship, Ship::rival, Region::projectile_mount, 20});
    CHECK(arena->launch({Ship::rival, {-24, 0, 0}}) == Reject::disabled);
    CHECK(arena->snapshot() == Snapshot{Health{65, 20, 20}, Health{100, 0, 0}});
}

TEST_CASE("saturation and later destruction cannot discard accepted projectiles", "[combat]") {
    auto arena = Arena::create({});
    REQUIRE(arena);
    REQUIRE_FALSE(arena->launch({Ship::player, {48, 0, 0}}));
    REQUIRE_FALSE(arena->launch({Ship::player, {48, 0, 0}}));
    REQUIRE_FALSE(arena->launch({Ship::player, {48, 0, 0}}));
    // The fourth segment has already left the rival when its hull is destroyed.
    REQUIRE_FALSE(arena->launch({Ship::rival, {-24, 0, 0}}));
    CHECK(arena->launch({Ship::player, {48, 0, 0}}) == Reject::full);
    const auto report = arena->advance();
    REQUIRE(report.size == 4);
    CHECK(report.impacts[0] == Impact{Hit::ship, Ship::rival, Region::hull, 35});
    CHECK(report.impacts[1] == Impact{Hit::ship, Ship::rival, Region::hull, 35});
    CHECK(report.impacts[2] == Impact{Hit::ship, Ship::rival, Region::hull, 30});
    CHECK(report.impacts[3] == Impact{Hit::ship, Ship::player, Region::hull, 35});
    CHECK(arena->launch({Ship::rival, {-24, 0, 0}}) == Reject::disabled);
    CHECK(arena->fire({Ship::player, Ship::rival, Region::hull}) ==
          std::variant<Impact, Reject>{Reject::invalid_target});
    REQUIRE_FALSE(arena->launch({Ship::player, {48, 0, 0}}));
    const auto wreck = arena->advance();
    REQUIRE(wreck.size == 1);
    CHECK(wreck.impacts[0] == Impact{Hit::ship, Ship::rival, Region::hull, 0});
    CHECK(arena->snapshot() == Snapshot{Health{65, 20, 20}, Health{0, 20, 20}});
    CHECK(arena->advance().size == 0);
}

TEST_CASE("invalid projectile paths never reserve a slot", "[combat]") {
    CHECK_FALSE(Arena::create({.separation_metres = 7}));
    CHECK_FALSE(Arena::create({.separation_metres = 129}));
    auto arena = Arena::create({});
    REQUIRE(arena);
    CHECK(arena->launch({Ship::none, {48, 0, 0}}) == Reject::invalid_shooter);
    CHECK(arena->launch({Ship::player, {0, 0, 0}}) == Reject::invalid_path);
    CHECK(arena->launch({Ship::player, {97, 0, 0}}) == Reject::invalid_path);
    CHECK(arena->launch({Ship::rival, {std::numeric_limits<int>::min(), 0, 0}}) ==
          Reject::invalid_path);
    CHECK(arena->advance().size == 0);
    CHECK(arena->snapshot() == Snapshot{});
}
