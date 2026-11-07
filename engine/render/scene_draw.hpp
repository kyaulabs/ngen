#pragma once
#include "render/mesh.hpp"
#include "render/scene.hpp"

namespace ngen::render {
struct SceneView {
    shader::Matrix view_projection;
    shader::Matrix light_view_projection;
    shader::Float4 camera_position{};
    shader::Float4 key_direction_intensity{0, -1, 0, 1};
    shader::Float4 key_color{1, 1, 1, 0};
    std::uint32_t lod{2};
};
struct SceneTargets {
    const Image &color;
    const Image &depth;
    const Image &shadow;
    const Image &irradiance;
    const Image &prefiltered;
    const Image &brdf;
    const Image &default_texture;
};
// Owner-thread bounded scene/version bridge. Constants are copied, while two
// successful frame pins retain immutable CPU/GPU scene metadata until completion.
class SceneRenderer {
  public:
    SceneRenderer();
    ~SceneRenderer();
    SceneRenderer(const SceneRenderer &) = delete;
    SceneRenderer &operator=(const SceneRenderer &) = delete;
    // Extra opaque/masked meshes share this view/lighting/schedule; their geometry
    // and instance spans borrow only for this call and join Context retirement.
    [[nodiscard]] std::variant<Submission, Failure>
    draw(Context &context, ScenePin version, const SceneView &view, SceneTargets targets,
         TimelineWait wait = {}, std::span<const MeshDraw> extras = {});
    [[nodiscard]] std::variant<std::size_t, Failure> collect(Context &context);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ngen::render
