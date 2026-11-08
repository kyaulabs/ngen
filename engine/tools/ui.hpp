#pragma once
#include "render/scene_draw.hpp"
#include "tools/inspector.hpp"
#include <string_view>

namespace ngen::tools {
struct InspectorMetrics {
    std::uint64_t frame{};
    double frame_ms{};
    render::GpuTimings gpu;
    render::ResourceStatistics memory;
    bool reloading{};
    std::string_view status;
};
// Owner-thread frontend. Destroy before Context closes its SDL window. GPU atlas
// and copied overlay buffers retain their ordinary Context submission lifetime.
class InspectorUi {
  public:
    [[nodiscard]] static std::variant<std::unique_ptr<InspectorUi>, render::Failure>
    create(render::Context &context);
    ~InspectorUi();
    InspectorUi(const InspectorUi &) = delete;
    InspectorUi &operator=(const InspectorUi &) = delete;
    void event(const SDL_Event &event);
    void frame(Inspector &inspector, const InspectorSnapshot &snapshot,
               const render::SceneView &view, const InspectorMetrics &metrics);
    [[nodiscard]] std::variant<std::optional<render::Overlay>, render::Failure>
    overlay(VkExtent2D extent);
    [[nodiscard]] std::uint64_t allocation_count() const noexcept;

  private:
    InspectorUi();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ngen::tools
