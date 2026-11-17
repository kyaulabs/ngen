#pragma once
#include "core/values.hpp"
#include <array>
#include <cstddef>
#include <cstdint>

namespace ngen::flight {
inline constexpr std::size_t rcs_bank_count = 16;
using RcsLevels = std::array<float, rcs_bank_count>;
using RcsMix = std::array<std::array<float, 12>, rcs_bank_count>;
struct RcsBank {
    // Body-local socket-pair center relative to the center of mass, and total
    // force of the pair at unit firing level. Torque is position cross force.
    std::array<float, 3> position_metres{}, force_newtons{};
};
struct RcsDefinition {
    std::array<RcsBank, rcs_bank_count> banks{};
    // +Fx,-Fx,+Fy,-Fy,+Fz,-Fz,+Tx,-Tx,+Ty,-Ty,+Tz,-Tz.
    RcsMix mix{};
};
struct ShipDefinition;
// Controller demand in the interval's before-tick body frame, not firing state.
[[nodiscard]] Result<RcsLevels> rcs_demand(const ShipDefinition &ship,
                                           const std::array<float, 4> &rotation_xyzw,
                                           const std::array<float, 3> &world_force,
                                           const std::array<float, 3> &world_torque);
// Simulation-owned valve state, advanced once per successful 60 Hz tick.
// Rendering receives the published result; it never advances this state.
class RcsPulses {
  public:
    [[nodiscard]] Result<RcsLevels> step(const RcsLevels &demand);
    [[nodiscard]] RcsLevels levels() const { return output_; }
    void reset() noexcept;

  private:
    RcsLevels output_{};
    double budget_{};
    unsigned burn_left_{}, off_left_{};
    std::uint32_t pattern_{};
    bool active_{}, burning_{};
};
} // namespace ngen::flight
