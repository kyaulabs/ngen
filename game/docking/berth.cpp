#include "docking/berth.hpp"
#include <algorithm>
#include <limits>

namespace ngen::docking {
Berth::Berth(Vessel vessel, Facility facility, economy::MarketSnapshot opening)
    : vessel_(vessel), facility_(facility), market_(opening) {}

Snapshot Berth::snapshot() const {
    return {vessel_, facility_, phase_, ticket_, available_, market_.snapshot()};
}

void Berth::set_available(bool available) { available_ = available; }

Admission Berth::begin(Vessel vessel) {
    if (!vessel.id || vessel != vessel_)
        return Rejection::wrong_vessel;
    if (phase_ != Phase::flight)
        return Rejection::wrong_phase;
    if (!available_)
        return Rejection::unavailable;
    if (attempt_ == std::numeric_limits<std::uint64_t>::max())
        return Rejection::exhausted;
    ticket_ = Ticket{vessel, ++attempt_, facility_};
    phase_ = Phase::approach;
    return *ticket_;
}

std::optional<Rejection> Berth::check(Ticket ticket, Phase expected) const {
    if (!ticket_ || ticket != *ticket_)
        return Rejection::stale_ticket;
    if (phase_ != expected)
        return Rejection::wrong_phase;
    return std::nullopt;
}

std::optional<Rejection> Berth::capture(Ticket ticket, RelativeMotion motion) {
    if (const auto blocked = check(ticket, Phase::approach))
        return blocked;
    if (!available_)
        return Rejection::unavailable;
    if (motion.speed_mm_s > 200 || std::ranges::any_of(motion.offset_mm, [](std::int32_t offset) {
            return offset < -500 || offset > 500;
        }))
        return Rejection::unsafe_motion;
    phase_ = Phase::docked;
    return std::nullopt;
}
std::optional<Rejection> Berth::abort(Ticket ticket) {
    if (const auto blocked = check(ticket, Phase::approach))
        return blocked;
    ticket_.reset();
    phase_ = Phase::flight;
    return std::nullopt;
}
std::optional<Rejection> Berth::depart(Ticket ticket) {
    if (const auto blocked = check(ticket, Phase::docked))
        return blocked;
    phase_ = Phase::departure;
    return std::nullopt;
}

std::optional<Rejection> Berth::clear(Ticket ticket, OffsetMillimetres offset_mm) {
    if (const auto blocked = check(ticket, Phase::departure))
        return blocked;
    if (std::ranges::none_of(offset_mm,
                             [](std::int32_t offset) { return offset <= -2000 || offset >= 2000; }))
        return Rejection::unsafe_motion;
    ticket_.reset();
    phase_ = Phase::flight;
    return std::nullopt;
}
TradeOutcome Berth::trade(Ticket ticket, economy::Trade order) {
    if (const auto blocked = check(ticket, Phase::docked))
        return *blocked;
    return market_.apply(order);
}
} // namespace ngen::docking
