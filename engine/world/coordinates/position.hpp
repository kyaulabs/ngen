#pragma once
#include "core/values.hpp"
#include <array>
#include <cstdint>

namespace ngen::coordinates {
inline constexpr double cell_width_metres = 0x1p32;
inline constexpr double local_limit_metres = 0x1p20;

struct Axis {
    std::int64_t cell{};
    double offset_metres{};
    auto operator<=>(const Axis &) const = default;
};
using Position = std::array<Axis, 3>;

// Canonical offsets are in [-2^31, 2^31) metres. No absolute-double conversion.
[[nodiscard]] Result<Position> normalize(Position position);

struct FrameStamp {
    std::uint64_t id{}, epoch{};
    Tick tick{};
    auto operator<=>(const FrameStamp &) const = default;
};
struct Frame {
    FrameStamp stamp;
    Position origin;
    std::array<double, 3> origin_velocity_mps{};
};
struct Sample {
    FrameStamp stamp;
    std::array<double, 3> position_metres{}, velocity_mps{};
};

// Both nonrotating origins are sampled at the pose tick. Identical stamps must
// describe identical frame states. The result owns its values and target stamp.
[[nodiscard]] Result<Sample> transform(const Sample &sample, const Frame &source,
                                       const Frame &target);
} // namespace ngen::coordinates
