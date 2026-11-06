#pragma once
#include "render/resources.hpp"
#include "render/surface.hpp"
#include <SDL3/SDL.h>
#include <atomic>
#include <memory>
#include <string>
#include <variant>
#include <vulkan/vulkan.h>

namespace ngen::render {
enum class FrameOutcome : std::uint8_t { queued, skipped };
struct PresentationState {
    std::uint64_t generation{};
    VkExtent2D extent{};
};
class Presentation;
class Context {
  public:
    struct Options {
        bool validation{true};
        bool visible{true};
        bool presentation{false};
    };
    [[nodiscard]] static std::variant<std::unique_ptr<Context>, Failure> create(Options options);
    ~Context();
    [[nodiscard]] bool close() noexcept;
    // Owner-thread query; refresh after resize/scale changes. A zero drawable suspends rendering.
    [[nodiscard]] SurfaceSelection inspect_surface() const;
    // Owner-thread first light. Queued is not proof that pixels reached a display.
    [[nodiscard]] std::variant<FrameOutcome, Failure> render_frame();
    [[nodiscard]] PresentationState presentation_state() const;
    [[nodiscard]] std::variant<Buffer, Failure> create_buffer(BufferDescription description);
    [[nodiscard]] std::variant<Image, Failure> create_image(ImageDescription description);
    [[nodiscard]] std::variant<Submission, Failure> copy_buffer(BufferCopy description);
    [[nodiscard]] std::variant<Submission, Failure> copy_to_image(ImageUpload description);
    [[nodiscard]] std::variant<Submission, Failure> copy_from_image(ImageReadback description);
    // Fixed asynchronous HDR -> LDR pass; warm pipelines before the measured play loop.
    // Uses the same bounded resource command contexts, timeline and retirement as transfers.
    [[nodiscard]] std::variant<Submission, Failure> post_process(PostProcess description);
    [[nodiscard]] std::variant<bool, Failure> submission_complete(const Submission &submission);
    [[nodiscard]] std::variant<ResourceStatistics, Failure> resource_statistics();
    // Sticky shutdown failure; validation errors are reported separately.
    [[nodiscard]] VkResult close_result() const { return close_result_; }
    Context(const Context &) = delete;
    Context &operator=(const Context &) = delete;
    [[nodiscard]] SDL_Window *window() const { return window_; }
    [[nodiscard]] VkInstance instance() const { return instance_; }
    [[nodiscard]] VkPhysicalDevice physical_device() const { return physical_; }
    [[nodiscard]] VkDevice device() const { return device_; }
    // Borrowed handles clear after teardown; unfinished retirement retains ownership for retry.
    [[nodiscard]] VkQueue queue() const { return queue_; }
    [[nodiscard]] VkSurfaceKHR surface() const { return surface_; }
    [[nodiscard]] std::uint32_t queue_family() const { return queue_family_; }
    [[nodiscard]] unsigned validation_errors() const { return validation_errors_.load(); }

  private:
    Context();
    void initialize(Options options);
    void destroy_messenger() noexcept;
    static VKAPI_ATTR VkBool32 VKAPI_CALL message(VkDebugUtilsMessageSeverityFlagBitsEXT,
                                                  VkDebugUtilsMessageTypeFlagsEXT,
                                                  const VkDebugUtilsMessengerCallbackDataEXT *,
                                                  void *);
    bool video_{};
    bool presentation_enabled_{};
    bool presentation_failed_{};
    SDL_Window *window_{};
    VkInstance instance_{};
    VkDebugUtilsMessengerEXT messenger_{};
    VkSurfaceKHR surface_{};
    VkPhysicalDevice physical_{};
    VkDevice device_{};
    VkQueue queue_{};
    std::uint32_t queue_family_{UINT32_MAX};
    VkResult close_result_{VK_SUCCESS};
    std::atomic<unsigned> validation_errors_{};
    // Destroy borrowers before every native-handle and callback-state subobject.
    std::shared_ptr<Resources> resources_;
    std::unique_ptr<Presentation> presentation_;
};
} // namespace ngen::render
