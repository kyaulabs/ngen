#pragma once
#include "render/resources.hpp"
#include <array>
#include <stop_token>
#include <vector>

namespace ngen::render {
struct Sky {
    std::array<float, 3> horizon{.12F, .15F, .2F};
    std::array<float, 3> zenith{.04F, .08F, .16F};
    std::array<float, 3> ground{.015F, .012F, .01F};
    float intensity{1};
};
struct EnvironmentImage {
    ImageDescription description;
    std::vector<std::byte> texels;
};
struct EnvironmentData {
    // Linear RGBA16F, native increasing-mip/face packing: diffuse8 cube,
    // prefiltered16 cube with five levels, then the 64x64 split-sum BRDF LUT.
    std::array<EnvironmentImage, 3> images;
};
[[nodiscard]] std::variant<EnvironmentData, Failure>
generate_environment(const Sky &sky, const std::stop_token &stop = {});
} // namespace ngen::render
