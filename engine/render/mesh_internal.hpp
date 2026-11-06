#pragma once
#include "render/mesh.hpp"
#include <vector>

namespace ngen::render {
struct MeshNativeDraw {
    std::array<VkImageView, 5> maps{};
};
struct MeshRecording {
    VkCommandBuffer commands{};
    std::size_t slot{};
    VkBuffer uniform{}, vertices{}, indices{};
    VkExtent2D extent{};
    VkImageView shadow{}, irradiance{}, prefiltered{}, brdf{};
    std::span<const MeshDraw> draws;
    std::span<const MeshNativeDraw> bindings;
};
class MeshPipeline {
  public:
    static constexpr std::size_t draw_limit = 256;
    MeshPipeline(VkDevice device, VkPhysicalDevice physical);
    ~MeshPipeline();
    MeshPipeline(const MeshPipeline &) = delete;
    MeshPipeline &operator=(const MeshPipeline &) = delete;
    [[nodiscard]] VkDeviceSize uniform_bytes() const { return scratch_.size(); }
    [[nodiscard]] std::span<const std::byte> prepare(const MeshPass &pass);
    void record(const MeshRecording &recording);
    void close() noexcept;

  private:
    VkSampler sampler(const MeshSampler &description);
    VkDevice device_{};
    std::array<VkDescriptorSetLayout, 2> descriptor_layouts_{};
    VkPipelineLayout layout_{};
    VkDescriptorPool pool_{};
    std::array<VkDescriptorSet, 2 * draw_limit * 2> sets_{};
    std::array<VkPipeline, 6> pipelines_{};
    VkSampler shadow_sampler_{}, environment_sampler_{};
    struct SamplerEntry {
        MeshSampler description;
        VkSampler native{};
    };
    std::array<SamplerEntry, 64> samplers_{};
    std::size_t sampler_count_{};
    VkDeviceSize frame_stride_{}, light_offset_{}, material_offset_{}, material_stride_{};
    std::vector<std::byte> scratch_;
};
} // namespace ngen::render
