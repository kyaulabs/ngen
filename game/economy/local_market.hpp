#pragma once
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <variant>

namespace ngen::economy {
struct GameCredit {
    std::uint64_t subunits{};
    auto operator<=>(const GameCredit &) const = default;
};
struct Units {
    std::uint64_t count{};
    auto operator<=>(const Units &) const = default;
};
struct OperationId {
    std::uint64_t value{};
    auto operator<=>(const OperationId &) const = default;
};
enum class Account : std::uint8_t { unspecified, player, station };
struct Holdings {
    GameCredit credits;
    Units stock;
    auto operator<=>(const Holdings &) const = default;
};
struct MarketSnapshot {
    Holdings player, station;
    auto operator<=>(const MarketSnapshot &) const = default;
};
// Prices and buyer are resolved by trusted game rules, not by a remote player.
// The other account is the seller; this slice trades one fungible resource.
struct Trade {
    OperationId id;
    Account buyer{};
    Units quantity;
    GameCredit unit_price;
    auto operator<=>(const Trade &) const = default;
};
struct Receipt {
    OperationId id;
    GameCredit total;
    MarketSnapshot after;
    auto operator<=>(const Receipt &) const = default;
};
enum class Rejection : std::uint8_t {
    invalid,
    conflict,
    insufficient_credits,
    insufficient_stock,
    overflow,
    journal_full
};
using Outcome = std::variant<Receipt, Rejection>;

// In-memory state for one game/session owner. Calls, including snapshot(),
// must be serialized on its owner thread. No engine or service owns this state.
class LocalMarket {
  public:
    static constexpr std::size_t journal_capacity = 64;
    explicit LocalMarket(MarketSnapshot initial) : state_(initial) {}
    LocalMarket(const LocalMarket &) = delete;
    LocalMarket &operator=(const LocalMarket &) = delete;

    [[nodiscard]] MarketSnapshot snapshot() const { return state_; }
    // A nonzero ID permanently names its first admitted request and outcome
    // for this session. After saturation, only known IDs can be replayed.
    [[nodiscard]] Outcome apply(Trade trade);

  private:
    struct Entry {
        Trade request;
        Outcome outcome;
    };
    MarketSnapshot state_;
    std::array<Entry, journal_capacity> journal_{};
    std::size_t used_{};
};
} // namespace ngen::economy
