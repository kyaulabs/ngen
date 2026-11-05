#pragma once
#include "render/context.hpp"
#include <array>
#include <optional>

namespace ngen::render {
// Context-owned; every borrowed handle expires only after this object is closed.
class Presentation {
  public:
    explicit Presentation(Context &owner) : owner_(owner) {}
    ~Presentation();
    Presentation(const Presentation &) = delete;
    Presentation &operator=(const Presentation &) = delete;
    void initialize();
    [[nodiscard]] FrameOutcome draw();
    [[nodiscard]] PresentationState state() const { return {generation_, extent_}; }
    [[nodiscard]] VkResult close(VkResult device_idle) noexcept;

  private:
    static constexpr std::size_t frame_count = 2;
    static constexpr std::size_t image_limit = 16;
    struct Frame {
        VkCommandPool pool{};
        VkCommandBuffer commands{};
        VkFence submitted_fence{};
        VkFence acquired_fence{};
        std::optional<std::uint32_t> acquired;
        bool submitted{};
        bool suboptimal{};
    };
    struct Image {
        VkImage image{}; // Borrowed from the swapchain, never destroyed directly.
        VkImageView view{};
        VkSemaphore ready{};
        VkFence retired{};
        bool present_pending{};
        bool acquired{};
    };
    [[nodiscard]] bool retire(bool wait);
    void release_acquired();
    void destroy_swapchain() noexcept;
    void rebuild(const SurfacePlan &plan);
    void record(Frame &frame, const Image &image);
    Context &owner_;
    PFN_vkReleaseSwapchainImagesEXT release_images_{};
    VkSwapchainKHR swapchain_{};
    std::array<Frame, frame_count> frames_{};
    std::array<Image, image_limit> images_{};
    std::uint32_t image_count_{};
    std::size_t frame_index_{};
    VkExtent2D drawable_{};
    VkExtent2D extent_{};
    std::uint64_t generation_{};
    bool rebuild_requested_{true};
};
} // namespace ngen::render
