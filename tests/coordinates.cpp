#include "world/coordinates/position.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>

using namespace ngen;
using namespace ngen::coordinates;

TEST_CASE("Coordinates normalize half-cell ties without rounding the preceding value up",
          "[coordinates]") {
    const double half = cell_width_metres / 2;
    const double inside = std::nextafter(half, 0.0);
    const double outside = std::nextafter(-half, -std::numeric_limits<double>::infinity());
    const auto position = normalize({{{2, half}, {7, inside}, {8, outside}}});
    REQUIRE(std::holds_alternative<Position>(position));
    const auto &point = std::get<Position>(position);
    CHECK(point[0] == Axis{3, -half});
    CHECK(point[1] == Axis{7, inside});
    CHECK(point[2] == Axis{7, half - 0x1p-21});
    CHECK(normalize(point) == position);
}

TEST_CASE("Same-tick nonrotating transforms preserve signed local position and velocity",
          "[coordinates]") {
    constexpr auto hi = std::numeric_limits<std::int64_t>::max();
    constexpr auto lo = std::numeric_limits<std::int64_t>::min();
    constexpr double half = cell_width_metres / 2;
    const Frame source{{1, 7, {42}},
                       {{{hi - 1, half - 4.5}, {lo + 1, -half + 3.5}, {220'000'000'000, 0.75}}},
                       {12, -25, 400}};
    const Frame target{{2, 9, {42}},
                       {{{hi, -half + 7.25}, {lo, half - 2.25}, {220'000'000'000, -0.25}}},
                       {10, -30, 410}};
    const Sample point{source.stamp, {1.25, -0.25, 3}, {-7, 2, 0.25}};
    const auto calculated = transform(point, source, target);
    REQUIRE(std::holds_alternative<Sample>(calculated));
    const auto &result = std::get<Sample>(calculated);
    CHECK(result.stamp == target.stamp);
    CHECK(result.position_metres == std::array{-10.5, 5.5, 4.0});
    CHECK(result.velocity_mps == std::array{-5.0, 7.0, -9.75});
}

TEST_CASE("Coordinate normalization checks carry and signed cell limits", "[coordinates]") {
    constexpr auto hi = std::numeric_limits<std::int64_t>::max();
    constexpr auto lo = std::numeric_limits<std::int64_t>::min();
    constexpr double half = cell_width_metres / 2;
    constexpr auto inf = std::numeric_limits<double>::infinity();
    constexpr auto nan = std::numeric_limits<double>::quiet_NaN();
    constexpr auto tiny = std::numeric_limits<double>::denorm_min();
    const auto folded =
        normalize({{{hi, -0x1p95}, {2, -3 * cell_width_metres - 0.125}, {4, tiny}}});
    REQUIRE(std::holds_alternative<Position>(folded));
    CHECK(std::get<Position>(folded) == Position{{{-1, 0}, {-1, -0.125}, {4, tiny}}});
    const auto zero = normalize({{{7, -0.0}, {}, {}}});
    REQUIRE(std::holds_alternative<Position>(zero));
    CHECK_FALSE(std::signbit(std::get<Position>(zero)[0].offset_metres));
    CHECK(normalize({{{hi, half}, {}, {}}}) == Result<Position>{Error::overflow});
    CHECK(normalize({{{lo, std::nextafter(-half, -inf)}, {}, {}}}) ==
          Result<Position>{Error::overflow});
    CHECK(normalize({{{lo, 0x1p95}, {}, {}}}) == Result<Position>{Error::overflow});
    CHECK(normalize({{{0, -inf}, {}, {}}}) == Result<Position>{Error::invalid});
    CHECK(normalize({{{0, nan}, {}, {}}}) == Result<Position>{Error::invalid});
}

TEST_CASE("Local offsets survive at AU, light-year and galactic locations", "[coordinates]") {
    const Position anchors{{{35, -725'984'660}, {2'202'748, -148'748'608}, {-220'000'000'000, 0}}};
    auto nearby = anchors;
    for (auto &axis : nearby)
        axis.offset_metres += 0.3125;
    const Frame a{{1, 4, {900}}, anchors, {12, 12, 12}};
    const Frame b{{1, 5, {900}}, nearby, {10, 10, 10}};
    const auto result = transform({a.stamp, {0.125, 0.125, 0.125}, {1, 2, 3}}, a, b);
    REQUIRE(std::holds_alternative<Sample>(result));
    CHECK(std::get<Sample>(result).position_metres == std::array{-0.1875, -0.1875, -0.1875});
    CHECK(std::get<Sample>(result).velocity_mps == std::array{3.0, 4.0, 5.0});
}

TEST_CASE("Ten thousand analytic round trips retain the starting pose", "[coordinates]") {
    constexpr auto hi = std::numeric_limits<std::int64_t>::max();
    constexpr double half = cell_width_metres / 2;
    const Frame a{{5, 7, {900}}, {{{hi - 1, std::nextafter(half, 0.0)}, {}, {}}}, {-317.61, 0, 0}};
    const Frame b{{6, 1, {900}}, {{{hi, -half + 314.125}, {}, {}}}, {411.29, 0, 0}};
    const Sample start{a.stamp, {17.035, 0, 0}, {119.729, 0, 0}};
    auto local = start;
    double position_error = 0;
    double velocity_error = 0;
    // Reference shift is the quarter-micrometre gap to the cell edge plus 314.125 m.
    constexpr double outbound_position = 17.035 - 314.125 - 0x1p-22;
    constexpr double outbound_velocity = -609.171;
    for (unsigned trip = 0; trip < 10'000; ++trip) {
        const auto outbound = transform(local, a, b);
        REQUIRE(std::holds_alternative<Sample>(outbound));
        const auto &other = std::get<Sample>(outbound);
        REQUIRE(other.stamp == b.stamp);
        position_error =
            std::max(position_error, std::abs(other.position_metres[0] - outbound_position));
        velocity_error =
            std::max(velocity_error, std::abs(other.velocity_mps[0] - outbound_velocity));
        const auto inbound = transform(other, b, a);
        REQUIRE(std::holds_alternative<Sample>(inbound));
        local = std::get<Sample>(inbound);
        REQUIRE(local.stamp == a.stamp);
        position_error =
            std::max(position_error, std::abs(local.position_metres[0] - start.position_metres[0]));
        velocity_error =
            std::max(velocity_error, std::abs(local.velocity_mps[0] - start.velocity_mps[0]));
    }
    CAPTURE(position_error, velocity_error);
    CHECK(position_error <= 0.001);
    CHECK(velocity_error <= 0.0001);
}

TEST_CASE("Transforms reject stale, noncanonical and out-of-envelope samples", "[coordinates]") {
    constexpr auto hi = std::numeric_limits<std::int64_t>::max();
    constexpr auto lo = std::numeric_limits<std::int64_t>::min();
    constexpr auto inf = std::numeric_limits<double>::infinity();
    const Frame a{{1, 2, {3}}, {}, {}};
    const Sample origin{a.stamp, {}, {}};
    const auto error = [](Result<Sample> result) {
        REQUIRE(std::holds_alternative<Error>(result));
        return std::get<Error>(result);
    };
    CHECK(error(transform({{2, 2, {3}}, {}, {}}, a, a)) == Error::stale);
    CHECK(error(transform({{1, 1, {3}}, {}, {}}, a, a)) == Error::stale);
    CHECK(error(transform({{1, 2, {2}}, {}, {}}, a, a)) == Error::stale);
    CHECK(error(transform(origin, a, {{1, 2, {4}}, {}, {}})) == Error::order);
    CHECK(error(transform(origin, a, {a.stamp, {{{0, 1}, {}, {}}}, {}})) == Error::invalid);
    CHECK(error(transform(origin, a, {a.stamp, {}, {1, 0, 0}})) == Error::invalid);
    const FrameStamp target{2, 1, {3}};
    CHECK(error(transform(origin, a, {target, {{{0, cell_width_metres / 2}, {}, {}}}, {}})) ==
          Error::invalid);
    const Frame distant_a{a.stamp, {{{hi, 0}, {}, {}}}, {}};
    const Frame distant_b{target, {{{lo, 0}, {}, {}}}, {}};
    CHECK(error(transform(origin, distant_a, distant_b)) == Error::invalid);

    const Sample edge{a.stamp, {local_limit_metres, 0, 0}, {}};
    const Frame displaced{target, {{{0, -local_limit_metres}, {}, {}}}, {}};
    REQUIRE(std::holds_alternative<Sample>(transform(edge, a, a)));
    REQUIRE(std::holds_alternative<Sample>(transform(origin, a, displaced)));
    CHECK(error(transform(edge, a, displaced)) == Error::invalid);
    CHECK(error(transform(origin, a, {target, {{{0, -local_limit_metres - 1}, {}, {}}}, {}})) ==
          Error::invalid);
    CHECK(error(transform({a.stamp, {std::nextafter(local_limit_metres, inf), 0, 0}, {}}, a, a)) ==
          Error::invalid);
    CHECK(error(transform({a.stamp, {inf, 0, 0}, {}}, a, a)) == Error::invalid);
    CHECK(error(transform({a.stamp, {}, {inf, 0, 0}}, a, a)) == Error::invalid);
    CHECK(error(transform(origin, a, {target, {}, {inf, 0, 0}})) == Error::invalid);
    constexpr auto top = std::numeric_limits<double>::max();
    CHECK(error(transform(origin, {a.stamp, {}, {top, 0, 0}}, {target, {}, {-top, 0, 0}})) ==
          Error::invalid);
}
