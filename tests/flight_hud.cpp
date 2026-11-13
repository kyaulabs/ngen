#include "flight_demo/hud_layout.hpp"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("On-demand HUD windows fit every admitted viewport without changing capacities") {
    using namespace ngen::demo;
    for (const auto extent : {HudRect{0, 0, 320, 180}, HudRect{0, 0, 640, 360},
                              HudRect{0, 0, 1280, 720}, HudRect{0, 0, 1920, 1080}}) {
        for (const auto both : {false, true}) {
            const auto layout = hud_layout(extent.width, extent.height, both, both);
            for (const auto rect : {layout.controls, layout.performance, layout.readout}) {
                REQUIRE(rect.width > 0);
                REQUIRE(rect.height > 0);
                REQUIRE(rect.x >= 0);
                REQUIRE(rect.y >= 0);
                REQUIRE(rect.x + rect.width <= extent.width);
                REQUIRE(rect.y + rect.height <= extent.height);
            }
            if (both) {
                const auto a = layout.controls, b = layout.performance;
                REQUIRE((a.x + a.width <= b.x || b.x + b.width <= a.x || a.y + a.height <= b.y ||
                         b.y + b.height <= a.y));
            }
        }
    }
}
