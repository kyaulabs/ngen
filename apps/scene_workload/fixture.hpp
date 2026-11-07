#pragma once
#include "render/scene_draw.hpp"
#include <array>

namespace ngen::workload {
inline constexpr std::uint32_t fixture_seed = 0x4e47454e;
inline constexpr std::uint32_t replay_frames = 600;
inline constexpr std::size_t asteroid_triangles = 80;
struct Fixture {
    std::array<render::shader::Vertex, asteroid_triangles * 3> vertices{};
    std::array<std::uint32_t, asteroid_triangles * 3> indices{};
    std::array<render::shader::InstanceTransform, 100> instances{};
    assets::Bounds ship_bounds;
    std::array<float, 3> center{};
    float radius{};
    std::uint32_t seed{};
};
[[nodiscard]] std::variant<Fixture, render::Failure>
make_fixture(const assets::Bounds &bounds, std::uint32_t seed = fixture_seed);
[[nodiscard]] std::variant<render::SceneView, render::Failure>
view(const Fixture &fixture, std::uint32_t frame, VkExtent2D extent);
} // namespace ngen::workload
