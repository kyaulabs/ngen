#include "core/entities.hpp"
#include "core/values.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <type_traits>
using namespace ngen;
static_assert(!std::is_convertible_v<Tick, Millimetres>);
TEST_CASE("entity reuse invalidates stale handles and bounds storage") {
    Entities entities;
    auto first = std::get<Entity>(entities.create());
    REQUIRE(entities.position(first) != nullptr);
    REQUIRE(entities.position(Entity{}) == nullptr);
    REQUIRE(entities.position(Entity{999, 1}) == nullptr);
    REQUIRE(std::holds_alternative<std::monostate>(entities.destroy(first)));
    auto second = std::get<Entity>(entities.create());
    REQUIRE(second.slot == first.slot);
    REQUIRE(second.generation != first.generation);
    REQUIRE(entities.position(first) == nullptr);
    REQUIRE(std::get<Error>(entities.destroy(first)) == Error::stale);
    for (std::size_t i = 1; i < Entities::capacity; ++i)
        REQUIRE(std::holds_alternative<Entity>(entities.create()));
    REQUIRE(std::get<Error>(entities.create()) == Error::capacity);
}
TEST_CASE("generations reject invalid values and cannot wrap into an old identity") {
    REQUIRE(std::get<Error>(next_generation(0)) == Error::invalid);
    REQUIRE(std::get<Error>(next_generation(UINT64_MAX)) == Error::overflow);
    REQUIRE(std::get<std::uint64_t>(next_generation(UINT64_MAX - 1)) == UINT64_MAX);
}
TEST_CASE("unit conversion rejects nonfinite and out of range values") {
    REQUIRE(std::get<Millimetres>(metres(1.25)).value == 1250);
    REQUIRE(std::get<Millimetres>(metres(-1.25)).value == -1250);
    REQUIRE(std::get<Error>(metres(std::numeric_limits<double>::infinity())) == Error::invalid);
    REQUIRE(std::get<Error>(metres(std::numeric_limits<double>::quiet_NaN())) == Error::invalid);
    REQUIRE(std::get<Error>(metres(1e30)) == Error::overflow);
    REQUIRE(std::get<Error>(metres(0x1p63 / 1000.0)) == Error::overflow);
    REQUIRE(std::get<Millimetres>(metres(-0x1p63 / 1000.0)).value == INT64_MIN);
    REQUIRE(std::get<Millimetres>(metres(0.0009)).value == 0);
}
TEST_CASE("tick and distance arithmetic refuse wraparound") {
    REQUIRE(std::get<Tick>(next(Tick{5})).value == 6);
    REQUIRE(std::get<Error>(next(Tick{UINT64_MAX})) == Error::overflow);
    REQUIRE(std::get<Error>(add(Millimetres{INT64_MAX}, Millimetres{1})) == Error::overflow);
    REQUIRE(std::get<Error>(add(Millimetres{INT64_MIN}, Millimetres{-1})) == Error::overflow);
}
