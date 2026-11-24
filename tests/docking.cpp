#include "docking/berth.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace ngen::docking;
namespace economy = ngen::economy;

namespace {
constexpr Vessel vessel{74};
constexpr economy::MarketSnapshot initial{{{100}, {1}}, {{20}, {12}}};
constexpr economy::Trade buy{{1}, economy::Account::player, {3}, {7}};
constexpr economy::MarketSnapshot after_buy{{{79}, {4}}, {{41}, {9}}};
} // namespace

TEST_CASE("a berth opens its market only after the assigned vessel captures it", "[docking]") {
    auto facility = Facility::station;
    SECTION("station approach") {}
    SECTION("planetary orbital hub approach") { facility = Facility::orbital_hub; }
    Berth berth(vessel, facility, initial);
    CHECK(berth.snapshot().facility == facility);
    CHECK(berth.begin({75}) == Admission{Rejection::wrong_vessel});
    berth.set_available(false);
    CHECK(berth.begin(vessel) == Admission{Rejection::unavailable});
    CHECK(berth.snapshot().phase == Phase::flight);
    berth.set_available(true);
    const auto admission = berth.begin(vessel);
    REQUIRE(std::holds_alternative<Ticket>(admission));
    const auto ticket = std::get<Ticket>(admission);
    CHECK(berth.begin(vessel) == Admission{Rejection::wrong_phase});
    CHECK(berth.trade(ticket, buy) == TradeOutcome{Rejection::wrong_phase});
    CHECK(berth.snapshot().holdings == initial);
    CHECK(berth.capture(ticket, {{501, 0, 0}, 200}) == Rejection::unsafe_motion);
    CHECK(berth.capture(ticket, {{0, 0, 0}, 201}) == Rejection::unsafe_motion);
    CHECK(berth.snapshot().phase == Phase::approach);
    REQUIRE_FALSE(berth.capture(ticket, {{500, -500, 0}, 200}));
    CHECK(berth.snapshot().phase == Phase::docked);
    CHECK(berth.snapshot().vessel == vessel);
    CHECK(berth.snapshot().holdings == initial);
    const TradeOutcome bought = economy::Outcome{economy::Receipt{{1}, {21}, after_buy}};
    CHECK(berth.trade(ticket, buy) == bought);
    CHECK(berth.trade(ticket, buy) == bought);
    CHECK(berth.snapshot().holdings == after_buy);
}

TEST_CASE("abort releases a reserved berth and old approaches cannot capture again", "[docking]") {
    Berth berth(vessel, Facility::station, initial);
    const auto first = berth.begin(vessel);
    REQUIRE(std::holds_alternative<Ticket>(first));
    const auto cancelled = std::get<Ticket>(first);
    berth.set_available(false);
    CHECK(berth.capture(cancelled, {}) == Rejection::unavailable);
    CHECK(berth.snapshot().phase == Phase::approach);
    REQUIRE_FALSE(berth.abort(cancelled));
    CHECK(berth.snapshot() ==
          Snapshot{vessel, Facility::station, Phase::flight, {}, false, initial});
    CHECK(berth.begin(vessel) == Admission{Rejection::unavailable});
    berth.set_available(true);
    const auto next = berth.begin(vessel);
    REQUIRE(std::holds_alternative<Ticket>(next));
    const auto current = std::get<Ticket>(next);
    CHECK(current != cancelled);
    CHECK(berth.capture(cancelled, {}) == Rejection::stale_ticket);
    CHECK(berth.abort(cancelled) == Rejection::stale_ticket);
    CHECK(berth.trade(cancelled, buy) == TradeOutcome{Rejection::stale_ticket});
    CHECK(berth.depart(current) == Rejection::wrong_phase);
    CHECK_FALSE(berth.capture(current, {}));
    CHECK(berth.abort(current) == Rejection::wrong_phase);
    CHECK(berth.trade({{75}, current.attempt}, buy) == TradeOutcome{Rejection::stale_ticket});
    CHECK(berth.snapshot().holdings == initial);
}

TEST_CASE("departure closes trading before clearing and re-docking preserves the market",
          "[docking]") {
    Berth berth(vessel, Facility::orbital_hub, initial);
    const auto admitted = berth.begin(vessel);
    REQUIRE(std::holds_alternative<Ticket>(admitted));
    const auto ticket = std::get<Ticket>(admitted);
    CHECK(berth.capture(ticket, {{std::numeric_limits<std::int32_t>::min(), 0, 0}, 0}) ==
          Rejection::unsafe_motion);
    REQUIRE_FALSE(berth.capture(ticket, {}));
    const auto bought = berth.trade(ticket, buy);
    REQUIRE(bought == TradeOutcome{economy::Outcome{economy::Receipt{{1}, {21}, after_buy}}});
    CHECK(berth.clear(ticket, {2000, 0, 0}) == Rejection::wrong_phase);
    berth.set_available(false);
    CHECK(berth.trade(ticket, buy) == bought);
    REQUIRE_FALSE(berth.depart(ticket));
    CHECK(berth.snapshot().phase == Phase::departure);
    CHECK(berth.begin(vessel) == Admission{Rejection::wrong_phase});
    constexpr economy::Trade sell{{2}, economy::Account::station, {2}, {5}};
    CHECK(berth.trade(ticket, sell) == TradeOutcome{Rejection::wrong_phase});
    CHECK(berth.trade(ticket, buy) == TradeOutcome{Rejection::wrong_phase});
    CHECK(berth.clear(ticket, {1999, 0, 0}) == Rejection::unsafe_motion);
    REQUIRE_FALSE(berth.clear(ticket, {0, -2000, 0}));
    CHECK(berth.snapshot() ==
          Snapshot{vessel, Facility::orbital_hub, Phase::flight, {}, false, after_buy});
    CHECK(berth.trade(ticket, sell) == TradeOutcome{Rejection::stale_ticket});

    berth.set_available(true);
    const auto readmitted = berth.begin(vessel);
    REQUIRE(std::holds_alternative<Ticket>(readmitted));
    const auto current = std::get<Ticket>(readmitted);
    REQUIRE_FALSE(berth.capture(current, {}));
    CHECK(berth.depart(ticket) == Rejection::stale_ticket);
    CHECK(berth.trade(ticket, sell) == TradeOutcome{Rejection::stale_ticket});
    CHECK(berth.trade(current, buy) == bought);
    CHECK(berth.snapshot().holdings == after_buy);
    constexpr economy::MarketSnapshot after_sell{{{89}, {2}}, {{31}, {11}}};
    CHECK(berth.trade(current, sell) ==
          TradeOutcome{economy::Outcome{economy::Receipt{{2}, {10}, after_sell}}});
    CHECK(berth.snapshot().holdings == after_sell);
    CHECK(berth.snapshot().vessel == vessel);
}

TEST_CASE("a ticket from another facility cannot capture or trade", "[docking]") {
    Berth station(vessel, Facility::station, initial);
    Berth hub(vessel, Facility::orbital_hub, initial);
    const auto station_admission = station.begin(vessel);
    const auto hub_admission = hub.begin(vessel);
    REQUIRE(std::holds_alternative<Ticket>(station_admission));
    REQUIRE(std::holds_alternative<Ticket>(hub_admission));
    const auto station_ticket = std::get<Ticket>(station_admission);
    const auto hub_ticket = std::get<Ticket>(hub_admission);
    CHECK(hub.capture(station_ticket, {}) == Rejection::stale_ticket);
    CHECK(hub.snapshot().phase == Phase::approach);
    REQUIRE_FALSE(hub.capture(hub_ticket, {}));
    CHECK(hub.trade(station_ticket, buy) == TradeOutcome{Rejection::stale_ticket});
    CHECK(hub.snapshot().holdings == initial);
}
