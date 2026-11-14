#pragma once

namespace ngen::demo {
struct HudRect {
    float x{}, y{}, width{}, height{};
};
struct HudLayout {
    HudRect controls, performance, readout, reset;
    bool compact{};
};
// Controls/performance rectangles cap content-fitted windows, not their actual size.
// Screen-space layout only; no simulation, renderer, input or OS ownership.
[[nodiscard]] HudLayout hud_layout(float width, float height, bool controls_open,
                                   bool performance_open) noexcept;
} // namespace ngen::demo
