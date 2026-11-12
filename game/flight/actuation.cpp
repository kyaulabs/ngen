#include <Jolt/Jolt.h>

#include "flight/flight.hpp"
#include <algorithm>
#include <cmath>

namespace ngen::flight {
namespace {
constexpr unsigned minimum_burn_ticks = 6;
constexpr unsigned minimum_off_ticks = 3;
} // namespace
Result<RcsLevels> rcs_demand(const ShipDefinition &ship, const std::array<float, 4> &q,
                             const std::array<float, 3> &world_force,
                             const std::array<float, 3> &world_torque) {
    if (!ship.rcs)
        return Error::invalid;
    double norm = 0;
    for (const auto value : q) {
        if (!std::isfinite(value))
            return Error::invalid;
        norm += static_cast<double>(value) * value;
    }
    if (std::abs(norm - 1) > 1e-4)
        return Error::invalid;
    for (std::size_t axis = 0; axis < 3; ++axis)
        if (!std::isfinite(world_force[axis]) || !std::isfinite(world_torque[axis]) ||
            !std::isfinite(ship.maximum_force_newtons[axis]) ||
            !std::isfinite(ship.maximum_torque_newton_metres[axis]) ||
            ship.maximum_force_newtons[axis] <= 0 || ship.maximum_torque_newton_metres[axis] <= 0)
            return Error::invalid;
    for (const auto &row : ship.rcs->mix)
        for (const auto value : row)
            if (!std::isfinite(value) || value < 0 || value > 1)
                return Error::invalid;
    const auto inverse = JPH::Quat{q[0], q[1], q[2], q[3]}.Normalized().Conjugated();
    const auto force = inverse * JPH::Vec3{world_force[0], world_force[1], world_force[2]};
    const auto torque = inverse * JPH::Vec3{world_torque[0], world_torque[1], world_torque[2]};
    const std::array<float, 6> local{force.GetX(),  force.GetY(),  force.GetZ(),
                                     torque.GetX(), torque.GetY(), torque.GetZ()};
    RcsLevels output{};
    for (std::size_t axis = 0; axis < 6; ++axis) {
        if (!std::isfinite(local[axis]))
            return Error::invalid;
        if (axis == 2 && local[axis] < 0)
            continue;
        const auto limit = axis < 3 ? ship.maximum_force_newtons[axis]
                                    : ship.maximum_torque_newton_metres[axis - 3];
        const auto power = std::clamp(local[axis] / limit, -1.0F, 1.0F);
        const auto column = axis * 2 + (power < 0 ? 1U : 0U);
        for (std::size_t bank = 0; bank < rcs_bank_count; ++bank)
            output[bank] += ship.rcs->mix[bank][column] * std::abs(power);
    }
    const auto largest = std::max(1.0F, *std::max_element(output.begin(), output.end()));
    for (auto &value : output)
        value /= largest;
    return output;
}
Result<RcsLevels> RcsPulses::step(const RcsLevels &demand) {
    for (const auto value : demand)
        if (!std::isfinite(value) || value < 0 || value > 1)
            return Error::invalid;
    const auto peak = *std::max_element(demand.begin(), demand.end());
    if (peak <= (active_ ? .0001F : .0002F)) {
        reset();
        return output_;
    }
    std::uint32_t pattern = 0;
    for (std::size_t bank = 0; bank < rcs_bank_count; ++bank)
        if (demand[bank] > peak * .01F)
            pattern |= std::uint32_t{1} << bank;
    // A fresh opposing couple must not wait for the old couple's off credit.
    // Ignore negligible helper/rounding levels when recognizing the pattern.
    if (active_ && (pattern_ & pattern) == 0)
        reset();
    pattern_ = pattern;
    if (peak >= 1) {
        if (!active_ || !burning_)
            burn_left_ = minimum_burn_ticks;
        if (burn_left_)
            --burn_left_;
        active_ = burning_ = true;
        budget_ = 0;
        off_left_ = 0;
        output_ = demand;
        return output_;
    }
    if (!active_) {
        active_ = burning_ = true;
        burn_left_ = minimum_burn_ticks;
    }
    const auto duty = std::sqrt(std::sqrt(static_cast<double>(peak)));
    const auto capacity = static_cast<double>(minimum_burn_ticks);
    budget_ = std::clamp(budget_ + duty, -capacity, capacity);
    if (burning_) {
        if (burn_left_)
            --burn_left_;
        else if (budget_ < 1) {
            burning_ = false;
            off_left_ = minimum_off_ticks;
        }
    }
    if (!burning_) {
        if (off_left_)
            --off_left_;
        else if (budget_ >= 0) {
            burning_ = true;
            burn_left_ = minimum_burn_ticks - 1;
        }
    }
    output_ = burning_ ? demand : RcsLevels{};
    if (burning_)
        budget_ -= 1;
    return output_;
}
void RcsPulses::reset() noexcept { *this = RcsPulses{}; }
} // namespace ngen::flight
