#pragma once
#include "economy/local_market.hpp"
#include <array>
#include <compare>
#include <cstdint>
#include <optional>
#include <variant>

namespace ngen::docking {
struct Vessel {
    std::uint64_t id{};
    auto operator<=>(const Vessel &) const = default;
};
enum class Facility : std::uint8_t { station, orbital_hub };
enum class Phase : std::uint8_t { flight, approach, docked, departure };
struct Ticket {
    Vessel vessel;
    std::uint64_t attempt{};
    Facility facility{Facility::station};
    auto operator<=>(const Ticket &) const = default;
};
using OffsetMillimetres = std::array<std::int32_t, 3>;
struct RelativeMotion {
    OffsetMillimetres offset_mm{};
    std::uint32_t speed_mm_s{};
};
struct Snapshot {
    Vessel vessel;
    Facility facility;
    Phase phase{Phase::flight};
    std::optional<Ticket> ticket{};
    bool available{true};
    economy::MarketSnapshot holdings;
    auto operator<=>(const Snapshot &) const = default;
};
enum class Rejection : std::uint8_t {
    wrong_vessel,
    stale_ticket,
    wrong_phase,
    unavailable,
    unsafe_motion,
    exhausted
};
using Admission = std::variant<Ticket, Rejection>;
using TradeOutcome = std::variant<economy::Outcome, Rejection>;

// One serialized game owner. Relative motion is from the trusted scripted
// approach in the berth's local frame, not a client-supplied pose or trade proof.
// Tickets are fixture-local; this is not a credential or cross-session ID.
class Berth {
  public:
    Berth(Vessel vessel, Facility facility, economy::MarketSnapshot opening);
    [[nodiscard]] Snapshot snapshot() const;
    // Admission and capture availability; existing docking is not revoked.
    void set_available(bool available);
    [[nodiscard]] Admission begin(Vessel vessel);
    [[nodiscard]] std::optional<Rejection> capture(Ticket ticket, RelativeMotion motion);
    [[nodiscard]] std::optional<Rejection> abort(Ticket ticket);
    [[nodiscard]] std::optional<Rejection> depart(Ticket ticket);
    [[nodiscard]] std::optional<Rejection> clear(Ticket ticket, OffsetMillimetres offset);
    [[nodiscard]] TradeOutcome trade(Ticket ticket, economy::Trade order);

  private:
    [[nodiscard]] std::optional<Rejection> check(Ticket ticket, Phase expected) const;

    const Vessel vessel_;
    const Facility facility_;
    Phase phase_{Phase::flight};
    std::optional<Ticket> ticket_;
    bool available_{true};
    std::uint64_t attempt_{};
    economy::LocalMarket market_;
};
} // namespace ngen::docking
