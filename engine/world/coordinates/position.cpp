#include "world/coordinates/position.hpp"
#include <cmath>
#include <limits>

namespace ngen::coordinates {
namespace {
static_assert(std::numeric_limits<double>::is_iec559 && std::numeric_limits<double>::digits == 53 &&
              std::numeric_limits<double>::max_exponent == 1024);
constexpr double half_cell = cell_width_metres / 2;
constexpr auto min_cell = std::numeric_limits<std::int64_t>::min();
constexpr auto max_cell = std::numeric_limits<std::int64_t>::max();

Result<Axis> normalize_axis(Axis axis) {
    if (!std::isfinite(axis.offset_metres))
        return Error::invalid;
    // Split before adding or subtracting a half cell, which could round a
    // representable offset just below +half_cell to the wrong side of the tie.
    const auto quotient = std::trunc(axis.offset_metres / cell_width_metres);
    if (quotient < -0x1p63 || quotient >= 0x1p63)
        return Error::overflow;
    auto carry = static_cast<std::int64_t>(quotient);
    auto rest = axis.offset_metres - quotient * cell_width_metres;
    if (rest >= half_cell) {
        if (carry == max_cell)
            return Error::overflow;
        ++carry;
        rest -= cell_width_metres;
    } else if (rest < -half_cell) {
        if (carry == min_cell)
            return Error::overflow;
        --carry;
        rest += cell_width_metres;
    }
    if ((carry > 0 && axis.cell > max_cell - carry) || (carry < 0 && axis.cell < min_cell - carry))
        return Error::overflow;
    return Axis{axis.cell + carry, rest == 0 ? 0.0 : rest};
}

bool canonical(Axis axis) {
    return axis.offset_metres >= -half_cell && axis.offset_metres < half_cell;
}

bool local_distance(double value) { return std::abs(value) <= local_limit_metres; }

Result<double> relative_origin(Axis source, Axis target) {
    if (!canonical(source) || !canonical(target))
        return Error::invalid;
    double shift = 0;
    // Decrement only the larger index. This checks adjacency even when the
    // original subtraction would overflow at opposite signed limits.
    if (source.cell > target.cell) {
        if (source.cell - 1 != target.cell)
            return Error::invalid;
        shift = cell_width_metres;
    } else if (target.cell > source.cell) {
        if (target.cell - 1 != source.cell)
            return Error::invalid;
        shift = -cell_width_metres;
    }
    const auto delta = shift + (source.offset_metres - target.offset_metres);
    if (!local_distance(delta))
        return Error::invalid;
    return delta;
}
} // namespace

Result<Position> normalize(Position position) {
    for (auto &axis : position) {
        const auto normalized = normalize_axis(axis);
        if (const auto *error = std::get_if<Error>(&normalized))
            return *error;
        axis = std::get<Axis>(normalized);
    }
    return position;
}

Result<Sample> transform(const Sample &sample, const Frame &source, const Frame &target) {
    if (sample.stamp != source.stamp)
        return Error::stale;
    if (source.stamp.tick != target.stamp.tick)
        return Error::order;
    if (source.stamp == target.stamp && (source.origin != target.origin ||
                                         source.origin_velocity_mps != target.origin_velocity_mps))
        return Error::invalid;

    Sample result{.stamp = target.stamp};
    for (std::size_t axis = 0; axis < sample.position_metres.size(); ++axis) {
        const auto source_velocity = source.origin_velocity_mps[axis];
        const auto target_velocity = target.origin_velocity_mps[axis];
        if (!local_distance(sample.position_metres[axis]) ||
            !std::isfinite(sample.velocity_mps[axis]) || !std::isfinite(source_velocity) ||
            !std::isfinite(target_velocity))
            return Error::invalid;
        const auto delta = relative_origin(source.origin[axis], target.origin[axis]);
        if (const auto *error = std::get_if<Error>(&delta))
            return *error;
        const auto local = std::get<double>(delta) + sample.position_metres[axis];
        const auto velocity = (source_velocity - target_velocity) + sample.velocity_mps[axis];
        if (!local_distance(local) || !std::isfinite(velocity))
            return Error::invalid;
        result.position_metres[axis] = local;
        result.velocity_mps[axis] = velocity;
    }
    return result;
}
} // namespace ngen::coordinates
