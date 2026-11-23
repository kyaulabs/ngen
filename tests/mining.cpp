#include "mining/site.hpp"
#include <barrier>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <thread>

using namespace ngen::mining;

TEST_CASE("claims reserve and transfer only available ore and hold space", "[mining]") {
    SECTION("the hold limits the yield") {
        Site site({.asteroid_capacity = {10}, .hold_capacity = {4}, .reserve = {8}});
        CHECK(site.apply({{1}, Claim{{6}}}) == Outcome{Receipt{{4}, {{6}, {4}, {8}}}});
        CHECK(site.apply({{2}, Claim{{1}}}) == Outcome{Rejection::hold_full});
        CHECK(site.snapshot() == Snapshot{{6}, {4}, {8}});
    }
    SECTION("the asteroid limits the yield") {
        Site site({.asteroid_capacity = {3}, .hold_capacity = {10}, .reserve = {8}});
        CHECK(site.apply({{1}, Claim{{6}}}) == Outcome{Receipt{{3}, {{0}, {3}, {8}}}});
        CHECK(site.apply({{2}, Claim{{1}}}) == Outcome{Rejection::depleted});
        CHECK(site.snapshot() == Snapshot{{0}, {3}, {8}});
    }
}

TEST_CASE("only an explicit replenishment event moves reserve ore into the asteroid", "[mining]") {
    Site site({.asteroid_capacity = {5}, .hold_capacity = {12}, .reserve = {8}});
    CHECK(site.apply({{1}, Replenish{{3}}}) == Outcome{Rejection::asteroid_full});
    REQUIRE(site.apply({{2}, Claim{{5}}}) == Outcome{Receipt{{5}, {{0}, {5}, {8}}}});
    CHECK(site.apply({{3}, Replenish{{2}}}) == Outcome{Receipt{{2}, {{2}, {5}, {6}}}});
    CHECK(site.apply({{4}, Replenish{{10}}}) == Outcome{Receipt{{3}, {{5}, {5}, {3}}}});
    REQUIRE(site.apply({{5}, Claim{{5}}}) == Outcome{Receipt{{5}, {{0}, {10}, {3}}}});
    CHECK(site.apply({{6}, Replenish{{5}}}) == Outcome{Receipt{{3}, {{3}, {10}, {0}}}});
    CHECK(site.apply({{7}, Replenish{{1}}}) == Outcome{Rejection::reserve_empty});
    const auto end = site.snapshot();
    CHECK(end == Snapshot{{3}, {10}, {0}});
    CHECK(end.asteroid.count + end.cargo.count + end.reserve.count == 13);
}

TEST_CASE("operation IDs pin successful and rejected results across later commands", "[mining]") {
    Site site({.asteroid_capacity = {3}, .hold_capacity = {10}, .reserve = {2}});
    constexpr Command claim{{1}, Claim{{3}}};
    const auto mined = site.apply(claim);
    REQUIRE(mined == Outcome{Receipt{{3}, {{0}, {3}, {2}}}});
    constexpr Command depleted{{2}, Claim{{1}}};
    REQUIRE(site.apply(depleted) == Outcome{Rejection::depleted});
    constexpr Command refill{{3}, Replenish{{2}}};
    const auto filled = site.apply(refill);
    REQUIRE(filled == Outcome{Receipt{{2}, {{2}, {3}, {0}}}});
    REQUIRE(site.apply({{4}, Claim{{1}}}) == Outcome{Receipt{{1}, {{1}, {4}, {0}}}});

    CHECK(site.apply(claim) == mined);
    CHECK(site.apply(depleted) == Outcome{Rejection::depleted});
    CHECK(site.apply(refill) == filled);
    CHECK(site.apply({claim.id, Claim{{1}}}) == Outcome{Rejection::conflict});
    CHECK(site.apply({refill.id, Claim{{2}}}) == Outcome{Rejection::conflict});
    CHECK(site.snapshot() == Snapshot{{1}, {4}, {0}});
}

TEST_CASE("concurrent callers cannot overdraw an asteroid or duplicate a claim", "[mining]") {
    Site site({.asteroid_capacity = {5}, .hold_capacity = {10}, .reserve = {7}});
    constexpr Command claim{{1}, Claim{{4}}};
    bool repeat = false;
    SECTION("two distinct claims contend for the final unit") {}
    SECTION("two copies of the same claim share a receipt") { repeat = true; }

    Outcome worker_result;
    std::barrier ready(2);
    std::jthread worker([&] {
        ready.arrive_and_wait();
        worker_result = site.apply(claim);
    });
    ready.arrive_and_wait();
    const auto main_result = site.apply(repeat ? claim : Command{{2}, Claim{{4}}});
    worker.join();
    if (repeat) {
        CHECK(worker_result == Outcome{Receipt{{4}, {{1}, {4}, {7}}}});
        CHECK(main_result == worker_result);
        CHECK(site.snapshot() == Snapshot{{1}, {4}, {7}});
    } else {
        const Outcome first = Receipt{{4}, {{1}, {4}, {7}}};
        const Outcome second = Receipt{{1}, {{0}, {5}, {7}}};
        CHECK(((worker_result == first && main_result == second) ||
               (worker_result == second && main_result == first)));
        CHECK(site.snapshot() == Snapshot{{0}, {5}, {7}});
    }
}

TEST_CASE("a full journal refuses new commands without losing replays", "[mining]") {
    constexpr auto cap = Site::journal_capacity;
    Site site({.asteroid_capacity = {cap + 1}, .hold_capacity = {cap + 1}, .reserve = {1}});
    CHECK(site.apply({{}, Claim{{1}}}) == Outcome{Rejection::invalid});
    constexpr Command invalid{{1}, Claim{{0}}};
    CHECK(site.apply(invalid) == Outcome{Rejection::invalid});
    constexpr Command first{{2}, Claim{{1}}};
    const auto original = site.apply(first);
    REQUIRE(std::holds_alternative<Receipt>(original));
    for (std::uint64_t id = 3; id <= cap; ++id)
        REQUIRE(std::holds_alternative<Receipt>(site.apply({{id}, Claim{{1}}})));

    CHECK(site.apply({{cap + 1}, Claim{{1}}}) == Outcome{Rejection::journal_full});
    CHECK(site.apply({{cap + 2}, Replenish{{1}}}) == Outcome{Rejection::journal_full});
    CHECK(site.apply(first) == original);
    CHECK(site.apply(invalid) == Outcome{Rejection::invalid});
    CHECK(site.apply({invalid.id, Claim{{1}}}) == Outcome{Rejection::conflict});
    CHECK(site.snapshot() == Snapshot{{2}, {cap - 1}, {1}});
}

TEST_CASE("each reservoir can reach the maximum unit count without wrapping", "[mining]") {
    constexpr auto max = std::numeric_limits<std::uint64_t>::max();
    Site site({.asteroid_capacity = {max}, .hold_capacity = {max}, .reserve = {max}});
    CHECK(site.apply({{1}, Claim{{max}}}) == Outcome{Receipt{{max}, {{0}, {max}, {max}}}});
    CHECK(site.apply({{2}, Replenish{{max}}}) == Outcome{Receipt{{max}, {{max}, {max}, {0}}}});
    CHECK(site.apply({{3}, Claim{{max}}}) == Outcome{Rejection::hold_full});
    CHECK(site.snapshot() == Snapshot{{max}, {max}, {0}});
}
