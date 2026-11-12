#pragma once
#include "flight_demo/preset.hpp"

namespace ngen::demo {
// Presentation only: one step per successful 60 Hz simulation tick. Render reads
// never advance firing. The physical wrench and approved asset stay unchanged.
class RcsPulses {
  public:
    [[nodiscard]] std::variant<RcsLevels, render::Failure> step(const RcsLevels &demand);
    [[nodiscard]] RcsLevels levels() const { return output_; }
    void reset() noexcept;

  private:
    RcsLevels output_{};
    double budget_{};
    unsigned burn_left_{}, off_left_{};
    bool active_{}, burning_{};
};
} // namespace ngen::demo
