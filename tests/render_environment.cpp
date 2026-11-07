#include "render/environment.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <cstring>
#include <limits>
#include <span>
#include <stop_token>

using namespace ngen::render;
namespace {
float half(std::span<const std::byte> bytes, std::size_t component) {
    std::uint16_t bits{};
    std::memcpy(&bits, bytes.data() + component * 2, 2);
    const auto word = static_cast<std::uint32_t>(bits);
    const auto exponent = (word >> 10U) & 31U, significand = word & 1023U;
    if (exponent == 31U)
        return std::numeric_limits<float>::infinity();
    const auto value = exponent == 0 ? std::ldexp(static_cast<float>(significand), -24)
                                     : std::ldexp(1 + static_cast<float>(significand) / 1024,
                                                  static_cast<int>(exponent) - 15);
    return (bits & 32768U) != 0 ? -value : value;
}
} // namespace
TEST_CASE("generated environment preserves constant radiance and cosine irradiance") {
    Sky sky;
    sky.horizon = sky.zenith = sky.ground = {.5F, .25F, .125F};
    sky.intensity = 2;
    const auto result = generate_environment(sky);
    REQUIRE(std::holds_alternative<EnvironmentData>(result));
    const auto &images = std::get<EnvironmentData>(result).images;
    REQUIRE(images[0].description.cube);
    REQUIRE(images[0].description.extent.width == 8);
    REQUIRE(images[0].texels.size() == 3072);
    REQUIRE(images[1].description.cube);
    REQUIRE(images[1].description.mip_levels == 5);
    REQUIRE(images[1].texels.size() == 16368);
    REQUIRE(images[2].description.extent.width == 64);
    REQUIRE_FALSE(images[2].description.cube);
    REQUIRE(images[2].texels.size() == 32768);
    const std::array<float, 3> radiance{1, .5F, .25F};
    for (std::size_t pixel = 0; pixel < images[1].texels.size() / 8; ++pixel)
        for (std::size_t c = 0; c < 3; ++c)
            REQUIRE(half(images[1].texels, pixel * 4 + c) == radiance[c]);
    for (std::size_t pixel = 0; pixel < images[0].texels.size() / 8; ++pixel)
        for (std::size_t c = 0; c < 3; ++c)
            REQUIRE(std::abs(half(images[0].texels, pixel * 4 + c) -
                             radiance[c] * 3.14159265358979323846F) < .002F);
}
TEST_CASE("generated cube orientation and roughness preserve sky direction") {
    Sky sky;
    sky.horizon = {0, 0, 0};
    sky.zenith = {1, 0, 0};
    sky.ground = {0, 0, 1};
    const auto result = generate_environment(sky);
    REQUIRE(std::holds_alternative<EnvironmentData>(result));
    const auto &spec = std::get<EnvironmentData>(result).images[1].texels;
    const auto center = [](std::size_t face) {
        return (face * 16 * 16 + std::size_t{7} * 16 + 7) * 4;
    };
    REQUIRE(half(spec, center(2)) > .99F);
    REQUIRE(half(spec, center(2) + 2) == 0);
    REQUIRE(half(spec, center(3) + 2) > .99F);
    REQUIRE(half(spec, center(3)) == 0);
    const auto last_mip = std::size_t{340} * 6 * 4;
    REQUIRE(half(spec, last_mip + 8) > .4F);
    REQUIRE(half(spec, last_mip + 8) < .85F);
}
TEST_CASE("split sum LUT is finite and has the smooth normal incidence limit") {
    const auto result = generate_environment(Sky{});
    REQUIRE(std::holds_alternative<EnvironmentData>(result));
    const auto &lut = std::get<EnvironmentData>(result).images[2].texels;
    for (std::size_t i = 0; i < lut.size() / 8; ++i) {
        const auto a = half(lut, i * 4), b = half(lut, i * 4 + 1);
        REQUIRE(std::isfinite(a));
        REQUIRE(std::isfinite(b));
        REQUIRE(a >= 0);
        REQUIRE(b >= 0);
        REQUIRE(a + b <= 1.25F);
    }
    REQUIRE(std::abs(half(lut, std::size_t{63} * 4) - 1) < .02F);
    REQUIRE(half(lut, std::size_t{63} * 4 + 1) < .001F);
    REQUIRE(std::abs(half(lut, 0) + half(lut, 1) - 1) < .05F);
    // For N=V and roughness=1, the split-sum visibility integrates to 1-ln(2).
    const auto corner = (std::size_t{63} * 64 + 63) * 4;
    REQUIRE(std::abs(half(lut, corner) + half(lut, corner + 1) - (1 - std::log(2.0F))) < .03F);
}
TEST_CASE("environment admission and cancellation never publish partial data") {
    Sky invalid;
    invalid.intensity = std::numeric_limits<float>::infinity();
    REQUIRE(std::holds_alternative<Failure>(generate_environment(invalid)));
    invalid = {};
    invalid.ground[0] = -.01F;
    REQUIRE(std::holds_alternative<Failure>(generate_environment(invalid)));
    invalid = {};
    invalid.zenith[2] = 17;
    REQUIRE(std::holds_alternative<Failure>(generate_environment(invalid)));
    std::stop_source stop;
    (void)stop.request_stop();
    REQUIRE(std::holds_alternative<Failure>(generate_environment(Sky{}, stop.get_token())));
}
