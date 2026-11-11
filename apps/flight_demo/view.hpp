#pragma once
#include "flight_demo/preset.hpp"
#include <optional>

namespace ngen::demo {
struct FrameView {
    render::SceneView scene;
    std::array<render::shader::InstanceTransform, 100> asteroids;
    std::array<double, 3> origin_metres{};
};
[[nodiscard]] std::variant<FrameView, render::Failure>
present(const Preset &preset, const flight::CameraFrame &camera, VkExtent2D extent);
// GUI-local pixels only for finite targets inside the full Vulkan clip volume.
[[nodiscard]] std::optional<std::array<float, 2>> project_target(const FrameView &view,
                                                                 const std::array<double, 3> &point,
                                                                 std::array<float, 2> size);
} // namespace ngen::demo
