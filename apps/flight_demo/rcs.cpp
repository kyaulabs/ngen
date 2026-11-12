#include "flight_demo/rcs.hpp"
#include <algorithm>
#include <cmath>

namespace ngen::demo {
namespace {
constexpr unsigned minimum_burn_ticks = 6;
constexpr unsigned minimum_off_ticks = 3;
} // namespace
std::variant<RcsLevels, render::Failure> RcsPulses::step(const RcsLevels &demand) {
    for (const auto value : demand)
        if (!std::isfinite(value) || value < 0 || value > 1)
            return render::Failure{"RCS pulse demand must contain finite unit levels"};
    const auto peak = *std::max_element(demand.begin(), demand.end());
    if (peak <= (active_ ? .0001F : .0002F)) {
        reset();
        return output_;
    }
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
        burn_left_ = minimum_burn_ticks; // 100 ms at 60 Hz; not a valve specification.
    }
    // Visual duty uses the existing perceptual response, not a physical impulse model.
    const auto duty = std::sqrt(std::sqrt(static_cast<double>(peak)));
    const auto capacity = static_cast<double>(minimum_burn_ticks);
    budget_ = std::clamp(budget_ + duty, -capacity, capacity);
    if (burning_) {
        if (burn_left_)
            --burn_left_;
        else if (budget_ < 1) {
            burning_ = false;
            off_left_ = minimum_off_ticks; // At least 50 ms off between correction puffs.
        }
    }
    if (!burning_) {
        if (off_left_)
            --off_left_;
        else if (budget_ >= 0) {
            burning_ = true;
            burn_left_ = minimum_burn_ticks - 1; // This tick is the first on tick.
        }
    }
    output_ = burning_ ? demand : RcsLevels{};
    if (burning_)
        budget_ -= 1;
    return output_;
}
void RcsPulses::reset() noexcept { *this = RcsPulses{}; }
} // namespace ngen::demo
