#include <vulkan/vulkan_core.h>

// Compile against the real pinned Vulkan headers under both host and Windows
// x64 ABIs. The negative control is the expression rejected by owner MSVC.
VkImageSubresourceRange image_range(bool depth) {
#if defined(NGEN_NARROW_CONTROL)
    const auto aspect = depth ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
#else
    const auto aspect = static_cast<VkImageAspectFlags>(depth ? VK_IMAGE_ASPECT_DEPTH_BIT
                                                              : VK_IMAGE_ASPECT_COLOR_BIT);
#endif
    return {aspect, 0, 1, 0, 1};
}
