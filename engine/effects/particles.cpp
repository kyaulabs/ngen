#include "effects/particles.hpp"
#include <algorithm>
#include <cmath>

namespace ngen::effects {
namespace {
constexpr double max_speed_mps = 500;
constexpr double ticks_per_second = 60;
constexpr std::array<double, 3> stationary{};

bool bounded(const std::array<double, 3> &value, double limit) {
    return std::all_of(value.begin(), value.end(),
                       [limit](double v) { return std::abs(v) <= limit; });
}
} // namespace

Result<Particles> Particles::create(const coordinates::Frame &frame) {
    if (frame.stamp.id == 0 || frame.stamp.epoch == 0 || frame.origin_velocity_mps != stationary)
        return Error::invalid;
    const auto valid = coordinates::transform({.stamp = frame.stamp}, frame, frame);
    if (const auto *error = std::get_if<Error>(&valid))
        return *error;
    return Particles(frame);
}

Result<std::monostate> Particles::emit(const coordinates::Sample &sample, unsigned lifetime) {
    if (sample.stamp != state_.frame.stamp)
        return Error::stale;
    if (lifetime == 0 || lifetime > max_lifetime_ticks ||
        !bounded(sample.position_metres, coordinates::local_limit_metres) ||
        !bounded(sample.velocity_mps, max_speed_mps))
        return Error::invalid;
    if (state_.count_ == state_.storage_.size())
        return Error::capacity;
    state_.storage_[state_.count_++] = {sample.position_metres, sample.velocity_mps, 0, lifetime};
    return std::monostate{};
}

Result<Tick> Particles::step() {
    const auto upcoming = next(state_.frame.stamp.tick);
    if (const auto *error = std::get_if<Error>(&upcoming))
        return *error;
    auto staged = state_;
    staged.count_ = 0;
    for (auto particle : state_.particles()) {
        if (++particle.age_ticks == particle.lifetime_ticks)
            continue;
        for (std::size_t axis = 0; axis < particle.position_metres.size(); ++axis)
            particle.position_metres[axis] += particle.velocity_mps[axis] / ticks_per_second;
        if (!bounded(particle.position_metres, coordinates::local_limit_metres))
            return Error::invalid;
        staged.storage_[staged.count_++] = particle;
    }
    staged.frame.stamp.tick = std::get<Tick>(upcoming);
    state_ = staged;
    return state_.frame.stamp.tick;
}

Result<std::monostate> Particles::rebase(const coordinates::Frame &frame) {
    const auto &source = state_.frame;
    if (frame.stamp.id != source.stamp.id)
        return Error::stale;
    if (frame.stamp.tick != source.stamp.tick)
        return Error::order;
    const auto upcoming = next_generation(source.stamp.epoch);
    if (const auto *error = std::get_if<Error>(&upcoming))
        return *error;
    if (frame.stamp.epoch != std::get<std::uint64_t>(upcoming))
        return Error::stale;
    if (frame.origin == source.origin || frame.origin_velocity_mps != stationary)
        return Error::invalid;
    // Validate the origin pair even with no live particles. Publish the new
    // epoch and all converted poses together, or leave the snapshot intact.
    const auto offset = coordinates::transform({.stamp = source.stamp}, source, frame);
    if (const auto *error = std::get_if<Error>(&offset))
        return *error;
    auto staged = state_;
    for (auto &particle : std::span(staged.storage_).first(staged.count_)) {
        const auto transformed = coordinates::transform(
            {source.stamp, particle.position_metres, particle.velocity_mps}, source, frame);
        if (const auto *error = std::get_if<Error>(&transformed))
            return *error;
        particle.position_metres = std::get<coordinates::Sample>(transformed).position_metres;
    }
    staged.frame = frame;
    state_ = staged;
    return std::monostate{};
}
} // namespace ngen::effects
