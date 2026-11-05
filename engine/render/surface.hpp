#pragma once
#include <cstdint>
#include <span>
#include <string>
#include <variant>
#include <vulkan/vulkan.h>

namespace ngen::render {
enum class SurfaceIssue : std::uint8_t {
    not_drawable,
    invalid_capabilities,
    unsupported_format,
    unsupported_present_mode,
    unsupported_usage,
    unsupported_alpha,
    query_failed,
    closed
};
struct SurfaceFailure {
    SurfaceIssue issue;
    VkResult result{VK_SUCCESS};
    std::string message{};
};
struct SurfacePlan {
    VkSurfaceFormatKHR format{};
    VkPresentModeKHR present_mode{VK_PRESENT_MODE_FIFO_KHR};
    VkExtent2D extent{};
    std::uint32_t image_count{};
    VkSurfaceTransformFlagBitsKHR transform{VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR};
    VkCompositeAlphaFlagBitsKHR alpha{VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR};
};
using SurfaceSelection = std::variant<SurfacePlan, SurfaceFailure>;
// Copies a plan, never retains the queried spans. Zero extent suspends rendering.
[[nodiscard]] SurfaceSelection select_surface(const VkSurfaceCapabilitiesKHR &capabilities,
                                              std::span<const VkSurfaceFormatKHR> formats,
                                              std::span<const VkPresentModeKHR> modes,
                                              VkExtent2D drawable);
} // namespace ngen::render
