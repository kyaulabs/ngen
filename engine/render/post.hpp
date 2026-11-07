#pragma once
#include "render/shader_contract.hpp"
#include <array>
#include <cstddef>
#include <vulkan/vulkan.h>

namespace ngen::render {
struct PostDraw {
    VkCommandBuffer commands{};
    std::size_t frame{};
    VkFormat format{};
    std::uint32_t format_padding{};
    VkImageView source{};
    VkExtent2D extent{};
    std::array<std::uint32_t, 2> constant_padding{};
    shader::PostConstants constants;
};
static_assert(sizeof(PostDraw) == 64 && offsetof(PostDraw, source) == 24 &&
              offsetof(PostDraw, constants) == 48);
// Resource-state owned. close() is called only after Context proves completion/loss.
class PostPipeline {
  public:
    explicit PostPipeline(VkDevice device);
    ~PostPipeline();
    PostPipeline(const PostPipeline &) = delete;
    PostPipeline &operator=(const PostPipeline &) = delete;
    void record(const PostDraw &draw) const;
    void close() noexcept;

  private:
    VkDevice device_{};
    VkDescriptorSetLayout descriptor_layout_{};
    VkPipelineLayout layout_{};
    VkDescriptorPool pool_{};
    VkSampler sampler_{};
    // Two resource command slots, then two WSI submission-fence slots.
    std::array<VkDescriptorSet, 4> sets_{};
    std::array<VkPipeline, 3> pipelines_{};
};
} // namespace ngen::render
