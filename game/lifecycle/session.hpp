#pragma once
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <optional>
#include <variant>

namespace ngen::lifecycle {
struct Life {
    std::uint64_t generation{};
    auto operator<=>(const Life &) const = default;
};
struct Cargo {
    std::uint64_t units{};
    auto operator<=>(const Cargo &) const = default;
};
struct Ship {
    Life life;
    Cargo cargo;
    auto operator<=>(const Ship &) const = default;
};
struct Wreck {
    Life from;
    Cargo cargo;
    auto operator<=>(const Wreck &) const = default;
};
struct Loss {
    Wreck created;
    auto operator<=>(const Loss &) const = default;
};
struct Recovery {
    Life granted;
    auto operator<=>(const Recovery &) const = default;
};
struct SalvageRequest {
    Life wreck;
    Life receiver;
};
struct Salvage {
    Life receiver;
    Cargo moved;
    auto operator<=>(const Salvage &) const = default;
};
enum class Reject : std::uint8_t { unknown_life, not_live, not_lost, empty, conflict, exhausted };
template <class T> using Outcome = std::variant<T, Reject>;

// Game-owned, offline fixture: one pilot, one fungible cargo, no sellable starter kit.
// Initial cargo is also the hold capacity of every replacement ship.
class Session {
  public:
    static constexpr std::size_t loss_capacity = 16;
    explicit Session(Cargo initial);

    [[nodiscard]] std::optional<Ship> ship() const;
    [[nodiscard]] std::optional<Wreck> wreck(Life from) const;
    // Call only from trusted game rules after the named ship is destroyed.
    [[nodiscard]] Outcome<Loss> record_loss(Life from);
    [[nodiscard]] Outcome<Recovery> recover(Life from);
    [[nodiscard]] Outcome<Salvage> salvage(SalvageRequest request);

  private:
    struct Record {
        Loss loss;
        std::optional<Salvage> salvage;
    };
    mutable std::mutex mutex_;
    std::optional<Ship> live_;
    std::array<Record, loss_capacity> losses_{};
    std::size_t used_{};
};
} // namespace ngen::lifecycle
