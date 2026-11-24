#include "lifecycle/session.hpp"

namespace ngen::lifecycle {
Session::Session(Cargo initial) : live_(Ship{{1}, initial}) {}

std::optional<Ship> Session::ship() const {
    const std::lock_guard lock(mutex_);
    return live_;
}

std::optional<Wreck> Session::wreck(Life from) const {
    const std::lock_guard lock(mutex_);
    if (!from.generation || from.generation > used_)
        return {};
    const auto &record = losses_[from.generation - 1];
    return Wreck{from, record.salvage ? Cargo{} : record.loss.created.cargo};
}

Outcome<Loss> Session::record_loss(Life from) {
    const std::lock_guard lock(mutex_);
    if (!from.generation)
        return Reject::unknown_life;
    if (from.generation <= used_)
        return losses_[from.generation - 1].loss;
    if (!live_ || live_->life != from)
        return Reject::unknown_life;
    // A live ship always has room for its loss; keep the guard at admission.
    if (used_ == losses_.size())
        return Reject::exhausted;
    const Loss loss{{from, live_->cargo}};
    losses_[used_++] = {loss, std::nullopt};
    live_.reset();
    return loss;
}

Outcome<Recovery> Session::recover(Life from) {
    const std::lock_guard lock(mutex_);
    if (!from.generation || from.generation > used_)
        return Reject::not_lost;
    const Life next{from.generation + 1};
    // A subsequent loss, or the next live generation, proves consumption.
    if (from.generation < used_ || live_)
        return Recovery{next};
    if (used_ == losses_.size())
        return Reject::exhausted;
    live_ = Ship{next, {}};
    return Recovery{next};
}

Outcome<Salvage> Session::salvage(SalvageRequest request) {
    const std::lock_guard lock(mutex_);
    const auto id = request.wreck.generation;
    if (!id || id > used_)
        return Reject::not_lost;
    auto &record = losses_[id - 1];
    if (record.salvage) {
        if (record.salvage->receiver != request.receiver)
            return Reject::conflict;
        return *record.salvage;
    }
    if (!live_ || live_->life != request.receiver)
        return Reject::not_live;
    const auto cargo = record.loss.created.cargo;
    if (!cargo.units)
        return Reject::empty;
    // A nonempty wreck owns the whole fixture stock, so the live hold is empty.
    live_->cargo = cargo;
    record.salvage = Salvage{request.receiver, cargo};
    return *record.salvage;
}
} // namespace ngen::lifecycle
