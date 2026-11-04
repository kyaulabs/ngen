#include "simulation/simulation.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace ngen;
TEST_CASE("command admission rejects wrong ticks, stale IDs and repeated sequences") {
    Simulation sim;
    auto entity = std::get<Entity>(sim.create());
    REQUIRE(std::get<Error>(sim.submit({Tick{2}, 1, entity, {1}})) == Error::order);
    REQUIRE(std::get<Error>(sim.submit({Tick{1}, 1, {}, {1}})) == Error::stale);
    REQUIRE(std::holds_alternative<std::monostate>(sim.submit({Tick{1}, 1, entity, {1}})));
    REQUIRE(std::get<Error>(sim.submit({Tick{1}, 1, entity, {1}})) == Error::order);
}
TEST_CASE("command capacity and overflow have no partial state effects") {
    Simulation sim;
    auto entity = std::get<Entity>(sim.create());
    for (std::uint64_t i = 1; i <= Simulation::command_capacity; ++i)
        REQUIRE(std::holds_alternative<std::monostate>(sim.submit({Tick{1}, i, entity, {1}})));
    REQUIRE(std::get<Error>(sim.submit({Tick{1}, 65, entity, {1}})) == Error::capacity);
    REQUIRE(std::get<Snapshot>(sim.step()).poses()[0].position.value == 64);
    REQUIRE(std::holds_alternative<std::monostate>(sim.submit({Tick{2}, 65, entity, {1}})));
    REQUIRE(std::holds_alternative<std::monostate>(sim.submit({Tick{2}, 66, entity, {INT64_MAX}})));
    REQUIRE(std::get<Error>(sim.step()) == Error::overflow);
    REQUIRE(sim.snapshot().tick().value == 1);
    REQUIRE(sim.snapshot().poses()[0].position.value == 64);
    REQUIRE(std::get<Snapshot>(sim.step()).tick().value == 2);
}

TEST_CASE("fixed tick applies ordered commands and publishes independent snapshots") {
    for (unsigned workers : {0U, 2U}) {
        Simulation sim(workers);
        auto entity = std::get<Entity>(sim.create());
        REQUIRE(std::holds_alternative<std::monostate>(
            sim.submit({Tick{1}, 1, entity, Millimetres{10}})));
        REQUIRE(std::holds_alternative<std::monostate>(
            sim.submit({Tick{1}, 2, entity, Millimetres{-3}})));
        auto first = std::get<Snapshot>(sim.step());
        REQUIRE(first.tick() == Tick{1});
        REQUIRE(first.poses()[0].position == Millimetres{7});
        REQUIRE(std::holds_alternative<std::monostate>(
            sim.submit({Tick{2}, 3, entity, Millimetres{5}})));
        auto second = std::get<Snapshot>(sim.step());
        REQUIRE(second.poses()[0].position == Millimetres{12});
        REQUIRE(first.poses()[0].position == Millimetres{7});
    }
}
