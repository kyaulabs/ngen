#include "economy/local_market.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace ngen::economy;

TEST_CASE("a local buy and sell conserve credits and stock in both accounts",
          "[economy-buy-sell]") {
    constexpr MarketSnapshot initial{{{100}, {1}}, {{20}, {12}}};
    LocalMarket market(initial);
    const auto buy = market.apply({{1}, Account::player, {3}, {7}});
    REQUIRE(std::holds_alternative<Receipt>(buy));
    CHECK(std::get<Receipt>(buy) == Receipt{{1}, {21}, {{{79}, {4}}, {{41}, {9}}}});
    CHECK(market.snapshot() == std::get<Receipt>(buy).after);

    const auto sell = market.apply({{2}, Account::station, {2}, {5}});
    REQUIRE(std::holds_alternative<Receipt>(sell));
    CHECK(std::get<Receipt>(sell) == Receipt{{2}, {10}, {{{89}, {2}}, {{31}, {11}}}});
    const auto end = market.snapshot();
    CHECK(end == std::get<Receipt>(sell).after);
    CHECK(end.player.credits.subunits + end.station.credits.subunits == 120);
    CHECK(end.player.stock.count + end.station.stock.count == 13);
}

TEST_CASE("replays return the original receipt and changed requests cannot reuse the ID",
          "[economy-replay]") {
    LocalMarket market({{{100}, {1}}, {{20}, {12}}});
    constexpr Trade original{{42}, Account::player, {3}, {7}};
    const auto first = market.apply(original);
    REQUIRE(std::holds_alternative<Receipt>(first));
    REQUIRE(std::holds_alternative<Receipt>(market.apply({{5}, Account::station, {2}, {5}})));
    const auto current = market.snapshot();
    CHECK(market.apply(original) == first);
    CHECK(market.apply({original.id, Account::station, {3}, {7}}) == Outcome{Rejection::conflict});
    CHECK(market.apply({original.id, Account::player, {2}, {7}}) == Outcome{Rejection::conflict});
    CHECK(market.apply({original.id, Account::player, {3}, {8}}) == Outcome{Rejection::conflict});
    CHECK(market.snapshot() == current);
}

TEST_CASE("insufficient credits are recorded without a partial transfer", "[economy-rejection]") {
    constexpr MarketSnapshot initial{{{1}, {1}}, {{50}, {12}}};
    LocalMarket market(initial);
    constexpr Trade purchase{{10}, Account::player, {2}, {3}};
    CHECK(market.apply(purchase) == Outcome{Rejection::insufficient_credits});
    CHECK(market.snapshot() == initial);

    REQUIRE(std::holds_alternative<Receipt>(market.apply({{11}, Account::station, {1}, {10}})));
    const auto funded = market.snapshot();
    CHECK(market.apply(purchase) == Outcome{Rejection::insufficient_credits});
    CHECK(market.apply({purchase.id, Account::player, {1}, {3}}) == Outcome{Rejection::conflict});
    CHECK(market.snapshot() == funded);
    REQUIRE(std::holds_alternative<Receipt>(market.apply({{12}, Account::player, {2}, {3}})));
    CHECK(market.snapshot() == MarketSnapshot{{{5}, {2}}, {{46}, {11}}});
}

TEST_CASE("out of stock and malformed trades preserve both accounts", "[economy-rejection]") {
    constexpr MarketSnapshot initial{{{20}, {0}}, {{10}, {2}}};
    LocalMarket market(initial);
    CHECK(market.apply({{1}, Account::player, {3}, {1}}) == Outcome{Rejection::insufficient_stock});
    CHECK(market.apply({{}, Account::player, {1}, {1}}) == Outcome{Rejection::invalid});
    CHECK(market.apply({{2}, Account::player, {0}, {1}}) == Outcome{Rejection::invalid});
    CHECK(market.apply({{3}, Account::player, {1}, {0}}) == Outcome{Rejection::invalid});
    CHECK(market.apply({{4}, Account::unspecified, {1}, {1}}) == Outcome{Rejection::invalid});
    CHECK(market.snapshot() == initial);
}

TEST_CASE("trade arithmetic checks the price and both receiving balances", "[economy-overflow]") {
    constexpr auto max = std::numeric_limits<std::uint64_t>::max();
    MarketSnapshot initial{{{max}, {0}}, {{0}, {2}}};
    Trade trade{{1}, Account::player, {2}, {max}};
    SECTION("the line total cannot wrap") {}
    SECTION("the receiving credit balance cannot wrap") {
        trade.unit_price = {1};
        initial.station.credits = {max - 1};
    }
    SECTION("the receiving stock cannot wrap") {
        trade.unit_price = {1};
        initial.player.stock = {max - 1};
    }
    LocalMarket market(initial);
    CHECK(market.apply(trade) == Outcome{Rejection::overflow});
    CHECK(market.snapshot() == initial);
}

TEST_CASE("capacity exhaustion cannot evict receipts or apply another trade",
          "[economy-capacity]") {
    constexpr auto cap = LocalMarket::journal_capacity;
    constexpr MarketSnapshot initial{{{cap + 1}, {0}}, {{0}, {cap + 1}}};
    LocalMarket market(initial);
    constexpr Trade first_trade{{1}, Account::player, {1}, {1}};
    const auto first = market.apply(first_trade);
    REQUIRE(std::holds_alternative<Receipt>(first));
    for (std::uint64_t id = 2; id < cap; ++id)
        REQUIRE(std::holds_alternative<Receipt>(market.apply({{id}, Account::player, {1}, {1}})));

    // A remembered rejection also occupies a slot; it cannot be evicted either.
    constexpr Trade rejected{{cap}, Account::player, {3}, {1}};
    CHECK(market.apply(rejected) == Outcome{Rejection::insufficient_credits});
    const auto saturated = market.snapshot();
    CHECK(market.apply({{cap + 1}, Account::player, {1}, {1}}) == Outcome{Rejection::journal_full});
    CHECK(market.apply(first_trade) == first);
    CHECK(market.apply(rejected) == Outcome{Rejection::insufficient_credits});
    CHECK(market.apply({first_trade.id, Account::player, {1}, {2}}) ==
          Outcome{Rejection::conflict});
    CHECK(market.snapshot() == saturated);
}
