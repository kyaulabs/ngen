#include "flight_demo/hud_layout.hpp"
#include <algorithm>

namespace ngen::demo {
HudLayout hud_layout(float width, float height, bool controls_open,
                     bool performance_open) noexcept {
    HudLayout result;
    result.compact = width < 800 || height < 400;
    result.controls = {8, 64, std::min(420.0F, width - 16), std::min(540.0F, height - 80)};
    result.performance = {width - std::min(350.0F, width - 16) - 8, 64,
                          std::min(350.0F, width - 16), std::min(312.0F, height - 80)};
    if (width < 800 && controls_open && performance_open) {
        const auto half = (height - 88) * .5F;
        result.controls.height = half;
        result.performance.y = 64 + half + 8;
        result.performance.height = half;
    }
    const auto readout_height = result.compact ? 84.0F : 116.0F;
    const auto readout_width = std::min(280.0F, width - 16);
    result.readout = {(width - readout_width) * .5F, height - readout_height - 8, readout_width,
                      readout_height};
    result.reset = {result.controls.x + result.controls.width * .5F + 4, result.controls.y + 80,
                    result.controls.width * .5F - 16, 28};
    return result;
}
} // namespace ngen::demo
