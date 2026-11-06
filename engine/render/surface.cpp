#include "render/surface.hpp"
#include <algorithm>
#include <array>
#include <bit>

namespace ngen::render {
SurfaceSelection select_surface(const VkSurfaceCapabilitiesKHR &capabilities,
                                std::span<const VkSurfaceFormatKHR> formats,
                                std::span<const VkPresentModeKHR> modes, VkExtent2D drawable) {
    if (drawable.width == 0 || drawable.height == 0)
        return SurfaceFailure{SurfaceIssue::not_drawable, VK_SUCCESS, "drawable extent is zero"};
    if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) == 0)
        return SurfaceFailure{SurfaceIssue::unsupported_usage, VK_SUCCESS,
                              "surface cannot be a color attachment"};
    constexpr std::array alpha_modes{
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR};
    const auto alpha = std::find_if(alpha_modes.begin(), alpha_modes.end(), [&](const auto mode) {
        return (capabilities.supportedCompositeAlpha &
                static_cast<VkCompositeAlphaFlagsKHR>(mode)) != 0;
    });
    if (alpha == alpha_modes.end())
        return SurfaceFailure{SurfaceIssue::unsupported_alpha, VK_SUCCESS,
                              "no supported composite alpha mode"};
    const bool variable = capabilities.currentExtent.width == UINT32_MAX;
    if (!std::has_single_bit(static_cast<std::uint32_t>(capabilities.currentTransform)) ||
        (capabilities.supportedTransforms &
         static_cast<VkSurfaceTransformFlagsKHR>(capabilities.currentTransform)) == 0 ||
        capabilities.minImageCount == 0 || capabilities.maxImageArrayLayers == 0 ||
        (capabilities.maxImageCount != 0 &&
         capabilities.maxImageCount < capabilities.minImageCount) ||
        capabilities.minImageExtent.width > capabilities.maxImageExtent.width ||
        capabilities.minImageExtent.height > capabilities.maxImageExtent.height ||
        variable != (capabilities.currentExtent.height == UINT32_MAX) ||
        (!variable && (capabilities.currentExtent.width < capabilities.minImageExtent.width ||
                       capabilities.currentExtent.width > capabilities.maxImageExtent.width ||
                       capabilities.currentExtent.height < capabilities.minImageExtent.height ||
                       capabilities.currentExtent.height > capabilities.maxImageExtent.height)))
        return SurfaceFailure{SurfaceIssue::invalid_capabilities, VK_SUCCESS,
                              "inconsistent surface capabilities"};
    const VkExtent2D extent =
        variable ? VkExtent2D{std::clamp(drawable.width, capabilities.minImageExtent.width,
                                         capabilities.maxImageExtent.width),
                              std::clamp(drawable.height, capabilities.minImageExtent.height,
                                         capabilities.maxImageExtent.height)}
                 : capabilities.currentExtent;
    if (extent.width == 0 || extent.height == 0)
        return SurfaceFailure{SurfaceIssue::not_drawable, VK_SUCCESS, "surface extent is zero"};
    constexpr std::array preferred{VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB};
    const bool unrestricted = formats.size() == 1 && formats[0].format == VK_FORMAT_UNDEFINED &&
                              formats[0].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    for (const auto format : preferred) {
        const auto found = std::find_if(formats.begin(), formats.end(), [format](const auto &f) {
            return f.format == format && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
        });
        if (unrestricted || found != formats.end()) {
            if (std::find(modes.begin(), modes.end(), VK_PRESENT_MODE_FIFO_KHR) == modes.end())
                return SurfaceFailure{SurfaceIssue::unsupported_present_mode, VK_SUCCESS,
                                      "FIFO presentation unavailable"};
            auto images = capabilities.minImageCount;
            if (images != UINT32_MAX)
                ++images;
            if (capabilities.maxImageCount != 0)
                images = std::min(images, capabilities.maxImageCount);
            return SurfacePlan{{format, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
                               VK_PRESENT_MODE_FIFO_KHR,
                               extent,
                               images,
                               capabilities.currentTransform,
                               *alpha};
        }
    }
    return SurfaceFailure{SurfaceIssue::unsupported_format, VK_SUCCESS,
                          "no nonlinear sRGB surface format"};
}
} // namespace ngen::render
