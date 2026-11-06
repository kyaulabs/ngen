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
    VkImageView source{};
    VkExtent2D extent{};
    shader::PostConstants constants;
};
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
    std::array<VkDescriptorSet, 2> sets_{};
    std::array<VkPipeline, 2> pipelines_{};
};
} // namespace ngen::render
