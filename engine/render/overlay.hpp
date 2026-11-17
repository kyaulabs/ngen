#pragma once
#include "render/resources.hpp"
#include <array>

namespace ngen::render {
struct OverlayRecording {
    VkCommandBuffer commands{};
    std::size_t slot{};
    VkFormat format{};
    VkExtent2D extent{};
    VkBuffer vertices{}, indices{};
    std::span<const OverlayDraw> draws;
};
// Native ownership follows Context completion, not this destructor's lexical scope.
class OverlayPipeline {
  public:
    explicit OverlayPipeline(VkDevice device);
    ~OverlayPipeline();
    OverlayPipeline(const OverlayPipeline &) = delete;
    OverlayPipeline &operator=(const OverlayPipeline &) = delete;
    void prepare(std::size_t slot, VkImageView font) const;
    void record(const OverlayRecording &recording) const;
    void close() noexcept;

  private:
    VkDevice device_{};
    VkDescriptorSetLayout descriptor_layout_{};
    VkPipelineLayout layout_{};
    VkDescriptorPool pool_{};
    VkSampler sampler_{};
    std::array<VkDescriptorSet, 4> sets_{};
    std::array<VkPipeline, 3> pipelines_{};
};
} // namespace ngen::render
