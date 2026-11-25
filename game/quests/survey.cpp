#include "quests/survey.hpp"
#include "combat/arena.hpp"
#include "docking/berth.hpp"
#include "lifecycle/session.hpp"
#include "mining/site.hpp"
#include "script/lua_rule.hpp"

#include <chrono>
#include <optional>

namespace ngen::quests {
namespace {
constexpr std::uint64_t trip_fee = 2;
constexpr std::uint64_t survey_bonus = 4;
constexpr std::uint64_t ore_price = 5;
constexpr std::uint64_t mining_limit = 3;
constexpr auto survey_rule = R"lua(
    local stage, action = ...
    if stage == 1 and action == 1 then return 1 end
    return 0
)lua";
static_assert(static_cast<unsigned>(Stage::enlistment) == 1);
static_assert(static_cast<unsigned>(Action::join_survey) == 1);

bool mine(State &state) {
    mining::Site site(
        {.asteroid_capacity = {state.deposit}, .hold_capacity = {mining_limit}, .reserve = {}});
    const auto outcome = site.apply({{1}, mining::Claim{{mining_limit}}});
    const auto *receipt = std::get_if<mining::Receipt>(&outcome);
    if (!receipt)
        return false;
    state.cargo = receipt->after.cargo.count;
    state.deposit = receipt->after.asteroid.count;
    state.stage = Stage::encounter;
    return true;
}

bool resolve_encounter(bool player_wins) {
    auto arena = combat::Arena::create({});
    if (!arena)
        return false;
    const auto winner = player_wins ? combat::Ship::player : combat::Ship::rival;
    const auto loser = player_wins ? combat::Ship::rival : combat::Ship::player;
    for (int n = 0; n < 5; ++n) {
        if (!std::holds_alternative<combat::Impact>(
                arena->fire({winner, loser, combat::Region::hull})))
            return false;
    }
    const auto health = arena->snapshot();
    return (player_wins ? health.rival.hull : health.player.hull) == 0;
}

bool lose(State &state) {
    if (!resolve_encounter(false))
        return false;
    lifecycle::Session ledger({state.cargo});
    const auto outcome = ledger.record_loss({1});
    const auto *loss = std::get_if<lifecycle::Loss>(&outcome);
    if (!loss || ledger.ship())
        return false;
    state.wreck = loss->created.cargo.units;
    state.cargo = 0;
    state.stage = Stage::lost;
    return true;
}

bool recover_or_salvage(State &state, Action action) {
    // Recreate the bounded two-life transaction from the canonical wreck.
    // Fixture-local IDs never replace the persistent campaign's vessel ID.
    lifecycle::Session ledger({state.wreck});
    if (!std::holds_alternative<lifecycle::Loss>(ledger.record_loss({1})) ||
        !std::holds_alternative<lifecycle::Recovery>(ledger.recover({1})))
        return false;
    if (action == Action::recover) {
        if (const auto starter = ledger.ship(); !starter || starter->cargo.units)
            return false;
        ++state.vessel;
        state.stage = Stage::recovered;
        return true;
    }
    const auto outcome = ledger.salvage({{1}, {2}});
    const auto *receipt = std::get_if<lifecycle::Salvage>(&outcome);
    const auto wreck = ledger.wreck({1});
    if (!receipt || !wreck || wreck->cargo.units || receipt->moved.units != state.wreck)
        return false;
    state.cargo = receipt->moved.units;
    state.wreck = 0;
    state.stage = Stage::returning;
    return true;
}

bool sell(State &state, std::optional<std::uint64_t> *market_ns) {
    const docking::Vessel ship{state.vessel};
    docking::Berth berth(
        ship, docking::Facility::station,
        {{{state.credits}, {state.cargo}}, {{state.station_credits}, {state.station_stock}}});
    const auto admitted = berth.begin(ship);
    const auto *ticket = std::get_if<docking::Ticket>(&admitted);
    if (!ticket || berth.capture(*ticket, {}))
        return false;
    using Clock = std::chrono::steady_clock;
    const auto started = market_ns ? Clock::now() : Clock::time_point{};
    const auto outcome =
        berth.trade(*ticket, {{1}, economy::Account::station, {state.cargo}, {ore_price}});
    if (market_ns)
        *market_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - started).count());
    const auto *market = std::get_if<economy::Outcome>(&outcome);
    const auto *receipt = market ? std::get_if<economy::Receipt>(market) : nullptr;
    if (!receipt || berth.depart(*ticket) || berth.clear(*ticket, {2000, 0, 0}))
        return false;
    state.cargo = receipt->after.player.stock.count;
    state.station_stock = receipt->after.station.stock.count;
    state.credits = receipt->after.player.credits.subunits;
    state.station_credits = receipt->after.station.credits.subunits;
    if (!state.completed_runs) {
        if (state.station_credits < survey_bonus)
            return false;
        state.station_credits -= survey_bonus;
        state.credits += survey_bonus;
    }
    ++state.completed_runs;
    state.stage = Stage::protected_port;
    return true;
}

std::optional<Rejection> transition(State &state, Action action,
                                    std::optional<std::uint64_t> *market_ns) {
    switch (action) {
    case Action::learn_law:
        if (state.stage != Stage::training)
            break;
        state.stage = Stage::enlistment;
        return {};
    case Action::join_survey:
        if (state.stage != Stage::enlistment)
            break;
        if (script::evaluate(survey_rule, {static_cast<std::uint8_t>(state.stage),
                                           static_cast<std::uint8_t>(action)}) !=
            script::Decision{std::uint8_t{1}})
            return Rejection::fixture_failure;
        state.stage = Stage::protected_port;
        return {};
    case Action::accept_risk:
        if (state.stage != Stage::protected_port)
            break;
        if (!state.deposit)
            return Rejection::exhausted;
        if (state.credits < trip_fee)
            return Rejection::fixture_failure;
        state.credits -= trip_fee;
        state.station_credits += trip_fee;
        state.stage = Stage::prospecting;
        return {};
    case Action::mine:
        if (state.stage == Stage::prospecting) {
            if (!mine(state))
                return Rejection::fixture_failure;
            return {};
        }
        break;
    case Action::defeat_rival:
        if (state.stage == Stage::encounter) {
            if (!resolve_encounter(true))
                return Rejection::fixture_failure;
            state.stage = Stage::returning;
            return {};
        }
        break;
    case Action::lose_ship:
        if (state.stage == Stage::encounter) {
            if (!lose(state))
                return Rejection::fixture_failure;
            return {};
        }
        break;
    case Action::recover:
    case Action::salvage:
        if (state.stage == (action == Action::recover ? Stage::lost : Stage::recovered)) {
            if (!recover_or_salvage(state, action))
                return Rejection::fixture_failure;
            return {};
        }
        break;
    case Action::dock_and_sell:
        if (state.stage == Stage::returning) {
            if (!sell(state, market_ns))
                return Rejection::fixture_failure;
            return {};
        }
        break;
    default:
        break;
    }
    return Rejection::wrong_stage;
}
} // namespace

std::span<const Action> Survey::history() const & { return std::span{history_}.first(used_); }

Outcome Survey::step(Command command, std::optional<std::uint64_t> *market_ns) {
    if (market_ns)
        market_ns->reset();
    if (command.expected > used_)
        return Rejection::order;
    if (command.expected < used_) {
        if (history_[command.expected] != command.action)
            return Rejection::conflict;
        return Receipt{results_[command.expected], command.expected + 1};
    }
    if (used_ == history_.size())
        return Rejection::exhausted;
    auto next = state_;
    if (const auto rejection = transition(next, command.action, market_ns))
        return *rejection;
    history_[used_] = command.action;
    results_[used_++] = next;
    state_ = next;
    return Receipt{state_, used_};
}
} // namespace ngen::quests
