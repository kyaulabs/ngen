#pragma once
#include "flight_demo/preset.hpp"
#include <filesystem>

namespace ngen::demo {
struct Options {
    VkExtent2D extent{1280, 720};
    std::uint32_t frames{}, warmup{60};
    bool verify{}, replay{}, walkthrough{}, validation{}, mute{};
    std::filesystem::path report, capture;
};
void run(const assets::PackagePin &package, const Preset &preset, const Options &options);
} // namespace ngen::demo
