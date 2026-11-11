#pragma once
#include "flight_demo/preset.hpp"

namespace ngen::demo {
struct FrameView {
    render::SceneView scene;
    std::array<render::shader::InstanceTransform, 100> asteroids;
    std::array<double, 3> origin_metres{};
};
[[nodiscard]] std::variant<FrameView, render::Failure>
present(const Preset &preset, const flight::CameraFrame &camera, VkExtent2D extent);
} // namespace ngen::demo
