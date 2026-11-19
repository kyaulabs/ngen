#pragma once
#include "world/coordinates/position.hpp"

namespace ngen::travel {
// Bounded linear path for committed travel, not a local contact trajectory.
class Trajectory {
  public:
    static constexpr std::uint64_t cell_delta_limit = 128;
    static constexpr std::uint32_t duration_limit = 60 * 60 * 60;
    [[nodiscard]] static Result<Trajectory> create(coordinates::Position start,
                                                   coordinates::Position destination,
                                                   std::uint32_t duration_ticks);
    [[nodiscard]] Result<coordinates::Position> sample(std::uint32_t elapsed_ticks) const;

  private:
    Trajectory() = default;
    coordinates::Position start_{}, destination_{};
    std::array<std::int64_t, 3> cells_{};
    std::uint32_t duration_{};
};
} // namespace ngen::travel
