#pragma once
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <variant>

namespace ngen::mining {
struct Units {
    std::uint64_t count{};
    auto operator<=>(const Units &) const = default;
};
struct OperationId {
    std::uint64_t value{};
    auto operator<=>(const OperationId &) const = default;
};
struct Snapshot {
    Units asteroid;
    Units cargo;
    Units reserve;
    auto operator<=>(const Snapshot &) const = default;
};
struct Claim {
    Units limit;
    auto operator<=>(const Claim &) const = default;
};
struct Replenish {
    Units limit;
    auto operator<=>(const Replenish &) const = default;
};
struct Command {
    OperationId id;
    std::variant<Claim, Replenish> action;
    auto operator<=>(const Command &) const = default;
};
struct Receipt {
    Units moved;
    Snapshot after;
    auto operator<=>(const Receipt &) const = default;
};
enum class Rejection : std::uint8_t {
    invalid,
    conflict,
    journal_full,
    hold_full,
    depleted,
    asteroid_full,
    reserve_empty
};
using Outcome = std::variant<Receipt, Rejection>;

struct SiteConfig {
    Units asteroid_capacity;
    Units hold_capacity;
    Units reserve;
};

// Offline game-owned fixture for one full asteroid, one empty hold and a finite reserve.
// Calls are serialized within the site; simultaneous callers have no priority guarantee.
class Site {
  public:
    static constexpr std::size_t journal_capacity = 64;
    explicit Site(SiteConfig config);
    [[nodiscard]] Snapshot snapshot() const;
    // Claims and trusted replenishment events share the same IDs. Each admitted
    // ID keeps its first terminal outcome, including rejections, until teardown.
    [[nodiscard]] Outcome apply(Command command);

  private:
    struct Entry {
        Command command;
        Outcome outcome;
    };
    [[nodiscard]] Outcome transfer(const Command &command);

    const Units asteroid_capacity_;
    const Units hold_capacity_;
    mutable std::mutex mutex_;
    Snapshot state_;
    std::array<Entry, journal_capacity> journal_{};
    std::size_t used_{};
};
} // namespace ngen::mining
