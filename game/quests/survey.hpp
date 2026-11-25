#pragma once
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <variant>

namespace ngen::quests {
enum class Stage : std::uint8_t {
    training,
    enlistment,
    protected_port,
    prospecting,
    encounter,
    lost,
    recovered,
    returning
};
enum class Action : std::uint8_t {
    learn_law = 0,
    join_survey = 1,
    accept_risk = 2,
    mine = 3,
    defeat_rival = 4,
    lose_ship = 5,
    recover = 6,
    salvage = 7,
    dock_and_sell = 8
};
struct State {
    Stage stage{Stage::training};
    std::uint64_t vessel{1};
    std::uint64_t credits{10}, station_credits{120};
    std::uint64_t deposit{12}, cargo{}, wreck{}, station_stock{};
    std::uint64_t completed_runs{};
    auto operator<=>(const State &) const = default;
};
struct Command {
    std::uint32_t expected{}; // Number of committed steps before this action.
    Action action{};
};
struct Receipt {
    State after;
    std::uint32_t revision{};
    auto operator<=>(const Receipt &) const = default;
};
enum class Rejection : std::uint8_t { wrong_stage, order, conflict, exhausted, fixture_failure };
using Outcome = std::variant<Receipt, Rejection>;

// Serialized game owner. Replay is scoped to this versioned campaign and never
// reaches another campaign, a persistent service, or the generic simulation.
class Survey {
  public:
    static constexpr std::size_t capacity = 64;
    [[nodiscard]] State snapshot() const { return state_; }
    [[nodiscard]] std::uint32_t revision() const { return used_; }
    // Borrowed view; its length is fixed at the time of the call.
    [[nodiscard]] std::span<const Action> history() const &;
    [[nodiscard]] std::span<const Action> history() const && = delete;
    [[nodiscard]] Outcome step(Command command);

  private:
    State state_;
    std::array<Action, capacity> history_{};
    std::array<State, capacity> results_{};
    std::uint32_t used_{};
};
} // namespace ngen::quests
