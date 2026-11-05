#pragma once
#include "render/surface.hpp"
#include <SDL3/SDL.h>
#include <atomic>
#include <memory>
#include <string>
#include <variant>
#include <vulkan/vulkan.h>

namespace ngen::render {
struct Failure {
    std::string message;
};
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
    // Destroy the borrower before every native-handle and callback-state subobject.
    std::unique_ptr<Presentation> presentation_;
};
} // namespace ngen::render
