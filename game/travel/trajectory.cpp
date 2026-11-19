#include "travel/trajectory.hpp"
#include <cmath>

namespace ngen::travel {
namespace {
bool canonical(const coordinates::Position &position) {
    const auto normalized = coordinates::normalize(position);
    return normalized == Result<coordinates::Position>{position};
}
} // namespace

Result<Trajectory> Trajectory::create(coordinates::Position start,
                                      coordinates::Position destination,
                                      std::uint32_t duration_ticks) {
    if (!duration_ticks || duration_ticks > duration_limit || !canonical(start) ||
        !canonical(destination) || start == destination)
        return Error::invalid;
    std::array<std::int64_t, 3> cells{};
    for (std::size_t i = 0; i < cells.size(); ++i) {
        const auto a = start[i].cell, b = destination[i].cell;
        // Unsigned subtraction retains the whole signed-domain distance.
        const auto distance = a <= b
                                  ? static_cast<std::uint64_t>(b) - static_cast<std::uint64_t>(a)
                                  : static_cast<std::uint64_t>(a) - static_cast<std::uint64_t>(b);
        if (distance > cell_delta_limit)
            return Error::invalid;
        cells[i] = b - a;
    }
    Trajectory result;
    result.start_ = start;
    result.destination_ = destination;
    result.cells_ = cells;
    result.duration_ = duration_ticks;
    return result;
}

Result<coordinates::Position> Trajectory::sample(std::uint32_t elapsed_ticks) const {
    if (elapsed_ticks > duration_)
        return Error::order;
    if (!elapsed_ticks)
        return start_;
    if (elapsed_ticks == duration_)
        return destination_;
    const auto fraction = static_cast<double>(elapsed_ticks) / duration_;
    auto position = start_;
    for (std::size_t i = 0; i < position.size(); ++i) {
        // Keep whole-cell travel exact; only the fractional cell becomes metres.
        const auto progress = cells_[i] * elapsed_ticks;
        position[i].cell += progress / duration_;
        position[i].offset_metres = std::fma(
            static_cast<double>(progress % duration_) / duration_, coordinates::cell_width_metres,
            std::lerp(start_[i].offset_metres, destination_[i].offset_metres, fraction));
    }
    return coordinates::normalize(position);
}
} // namespace ngen::travel
