#include "combat/arena.hpp"
#include "lifecycle/session.hpp"
#include <barrier>
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <thread>

using namespace ngen::lifecycle;

TEST_CASE("a destroyed fixture ship transfers its surviving cargo to exactly one wreck",
          "[lifecycle]") {
    namespace combat = ngen::combat;
    Session session({7});
    auto arena = combat::Arena::create({});
    REQUIRE(arena);
    REQUIRE(session.ship() == Ship{{1}, {7}});
    CHECK(session.record_loss({2}) == Outcome<Loss>{Reject::unknown_life});
    for (int n = 0; n < 5; ++n)
        REQUIRE(std::holds_alternative<combat::Impact>(
            arena->fire({combat::Ship::rival, combat::Ship::player, combat::Region::hull})));
    REQUIRE(arena->snapshot().player.hull == 0);
    const Outcome<Loss> loss = Loss{{{1}, {7}}};
    CHECK(session.record_loss({1}) == loss);
    CHECK_FALSE(session.ship());
    CHECK(session.wreck({1}) == Wreck{{1}, {7}});
    CHECK(session.record_loss({1}) == loss);
    CHECK_FALSE(session.wreck({2}));
}

TEST_CASE("each loss grants exactly one empty replacement without a cargo reward", "[lifecycle]") {
    Session session({7});
    CHECK(session.recover({1}) == Outcome<Recovery>{Reject::not_lost});
    for (std::uint64_t generation = 1; generation <= 4; ++generation) {
        const Life current{generation};
        const Life next{generation + 1};
        const Cargo on_loss{generation == 1 ? 7U : 0U};
        REQUIRE(session.record_loss(current) == Outcome<Loss>{Loss{{current, on_loss}}});
        CHECK(session.recover(current) == Outcome<Recovery>{Recovery{next}});
        CHECK(session.recover(current) == Outcome<Recovery>{Recovery{next}});
        CHECK(session.ship() == Ship{next, {0}});
        CHECK(session.record_loss(current) == Outcome<Loss>{Loss{{current, on_loss}}});
        CHECK(session.ship() == Ship{next, {0}});
    }
    CHECK(session.wreck({1}) == Wreck{{1}, {7}});
    for (std::uint64_t generation = 2; generation <= 4; ++generation)
        CHECK(session.wreck({generation}) == Wreck{{generation}, {0}});
}

TEST_CASE("salvage transfers existing cargo once to the named live generation", "[lifecycle]") {
    Session session({7});
    const SalvageRequest claim{{1}, {2}};
    CHECK(session.salvage(claim) == Outcome<Salvage>{Reject::not_lost});
    const auto death = session.record_loss({1});
    REQUIRE(std::holds_alternative<Loss>(death));
    CHECK(session.salvage(claim) == Outcome<Salvage>{Reject::not_live});
    REQUIRE(session.recover({1}) == Outcome<Recovery>{Recovery{{2}}});
    CHECK(session.salvage({{1}, {1}}) == Outcome<Salvage>{Reject::not_live});
    const Outcome<Salvage> transfer = Salvage{{2}, {7}};
    CHECK(session.salvage(claim) == transfer);
    CHECK(session.ship() == Ship{{2}, {7}});
    CHECK(session.wreck({1}) == Wreck{{1}, {0}});
    CHECK(session.record_loss({1}) == death);

    REQUIRE(session.record_loss({2}) == Outcome<Loss>{Loss{{{2}, {7}}}});
    REQUIRE(session.recover({2}) == Outcome<Recovery>{Recovery{{3}}});
    CHECK(session.salvage(claim) == transfer);
    CHECK(session.salvage({{1}, {3}}) == Outcome<Salvage>{Reject::conflict});
    CHECK(session.ship() == Ship{{3}, {0}});
    CHECK(session.wreck({2}) == Wreck{{2}, {7}});
}

TEST_CASE("concurrent retries and a salvage/death race cannot duplicate cargo", "[lifecycle]") {
    const Cargo stock{std::numeric_limits<std::uint64_t>::max()};
    Session session(stock);
    const auto race = [](auto left, auto right) {
        std::barrier start(2);
        std::jthread worker([&] {
            start.arrive_and_wait();
            left();
        });
        start.arrive_and_wait();
        right();
    };
    Outcome<Loss> first_loss, duplicate_loss;
    race([&] { first_loss = session.record_loss({1}); },
         [&] { duplicate_loss = session.record_loss({1}); });
    REQUIRE(first_loss == Outcome<Loss>{Loss{{{1}, stock}}});
    CHECK(duplicate_loss == first_loss);
    CHECK_FALSE(session.ship());

    Outcome<Recovery> first_recovery, duplicate_recovery;
    race([&] { first_recovery = session.recover({1}); },
         [&] { duplicate_recovery = session.recover({1}); });
    REQUIRE(first_recovery == Outcome<Recovery>{Recovery{{2}}});
    CHECK(duplicate_recovery == first_recovery);

    Outcome<Salvage> claimed;
    Outcome<Loss> second_loss;
    race([&] { claimed = session.salvage({{1}, {2}}); },
         [&] { second_loss = session.record_loss({2}); });
    CHECK_FALSE(session.ship());
    Life surviving_wreck{1};
    if (claimed == Outcome<Salvage>{Reject::not_live}) {
        CHECK(second_loss == Outcome<Loss>{Loss{{{2}, {0}}}});
        CHECK(session.wreck({1}) == Wreck{{1}, stock});
        CHECK(session.wreck({2}) == Wreck{{2}, {0}});
    } else {
        REQUIRE(claimed == Outcome<Salvage>{Salvage{{2}, stock}});
        CHECK(second_loss == Outcome<Loss>{Loss{{{2}, stock}}});
        CHECK(session.wreck({1}) == Wreck{{1}, {0}});
        CHECK(session.wreck({2}) == Wreck{{2}, stock});
        surviving_wreck = {2};
    }
    REQUIRE(session.recover({2}) == Outcome<Recovery>{Recovery{{3}}});
    const SalvageRequest retry{surviving_wreck, {3}};
    Outcome<Salvage> first_salvage, duplicate_salvage;
    race([&] { first_salvage = session.salvage(retry); },
         [&] { duplicate_salvage = session.salvage(retry); });
    CHECK(first_salvage == Outcome<Salvage>{Salvage{{3}, stock}});
    CHECK(duplicate_salvage == first_salvage);
    CHECK(session.ship() == Ship{{3}, stock});
    CHECK(session.wreck({1}) == Wreck{{1}, {0}});
    CHECK(session.wreck({2}) == Wreck{{2}, {0}});
}

TEST_CASE("fixture exhaustion preserves losses and replays without issuing an untracked ship",
          "[lifecycle]") {
    Session session({1});
    constexpr auto bound = Session::loss_capacity;
    for (std::uint64_t i = 1; i < bound; ++i) {
        REQUIRE(std::holds_alternative<Loss>(session.record_loss({i})));
        REQUIRE(session.recover({i}) == Outcome<Recovery>{Recovery{{i + 1}}});
    }
    CHECK(session.salvage({{2}, {bound}}) == Outcome<Salvage>{Reject::empty});
    const Outcome<Loss> last = Loss{{{bound}, {0}}};
    CHECK(session.record_loss({bound}) == last);
    CHECK(session.recover({bound}) == Outcome<Recovery>{Reject::exhausted});
    CHECK(session.record_loss({bound}) == last);
    CHECK(session.recover({1}) == Outcome<Recovery>{Recovery{{2}}});
    CHECK(session.wreck({1}) == Wreck{{1}, {1}});
    CHECK_FALSE(session.wreck({bound + 1}));
    CHECK_FALSE(session.ship());
}
