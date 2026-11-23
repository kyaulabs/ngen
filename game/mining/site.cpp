#include "mining/site.hpp"
#include <algorithm>
#include <span>

namespace ngen::mining {
Site::Site(SiteConfig config)
    : asteroid_capacity_(config.asteroid_capacity), hold_capacity_(config.hold_capacity),
      state_{config.asteroid_capacity, {}, config.reserve} {}

Snapshot Site::snapshot() const {
    const std::lock_guard lock(mutex_);
    return state_;
}

Outcome Site::apply(Command command) {
    if (!command.id.value)
        return Rejection::invalid;
    const std::lock_guard lock(mutex_);
    const auto entries = std::span{journal_}.first(used_);
    const auto existing =
        std::ranges::find(entries, command.id, [](const Entry &entry) { return entry.command.id; });
    if (existing != entries.end()) {
        if (existing->command != command)
            return Rejection::conflict;
        return existing->outcome;
    }
    if (used_ == journal_.size())
        return Rejection::journal_full;

    const auto result = transfer(command);
    journal_[used_++] = {command, result};
    return result;
}

Outcome Site::transfer(const Command &command) {
    const auto limit =
        std::visit([](const auto &action) { return action.limit.count; }, command.action);
    if (!limit)
        return Rejection::invalid;
    const bool claim = std::holds_alternative<Claim>(command.action);
    auto &source = claim ? state_.asteroid : state_.reserve;
    auto &destination = claim ? state_.cargo : state_.asteroid;
    const auto room = (claim ? hold_capacity_ : asteroid_capacity_).count - destination.count;
    if (!room)
        return claim ? Rejection::hold_full : Rejection::asteroid_full;
    if (!source.count)
        return claim ? Rejection::depleted : Rejection::reserve_empty;
    const auto moved = std::min({limit, room, source.count});
    source.count -= moved;
    destination.count += moved;
    return Receipt{Units{moved}, state_};
}
} // namespace ngen::mining
