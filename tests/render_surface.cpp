#include "render/surface.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>

using namespace ngen::render;
namespace {
VkSurfaceCapabilitiesKHR capabilities() {
    return {.minImageCount = 2,
            .maxImageCount = 0,
            .currentExtent = {UINT32_MAX, UINT32_MAX},
            .minImageExtent = {64, 64},
            .maxImageExtent = {4096, 4096},
            .maxImageArrayLayers = 1,
            .supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
            .supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
            .supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
}
constexpr std::array formats{
    VkSurfaceFormatKHR{VK_FORMAT_R8G8B8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR},
    VkSurfaceFormatKHR{VK_FORMAT_B8G8R8A8_SRGB, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
constexpr std::array modes{VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR};
} // namespace

TEST_CASE("surface selection prefers explicit sRGB and FIFO") {
    const auto result = select_surface(capabilities(), formats, modes, {1280, 720});
    REQUIRE(std::holds_alternative<SurfacePlan>(result));
    const auto &plan = std::get<SurfacePlan>(result);
    REQUIRE(plan.format.format == VK_FORMAT_B8G8R8A8_SRGB);
    REQUIRE(plan.format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR);
    REQUIRE(plan.present_mode == VK_PRESENT_MODE_FIFO_KHR);
    REQUIRE(plan.extent.width == 1280);
    REQUIRE(plan.extent.height == 720);
    REQUIRE(plan.image_count == 3);
    REQUIRE(plan.alpha == VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR);
    REQUIRE(plan.transform == VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);
}

TEST_CASE("undefined surface format permits the explicit sRGB choice") {
    std::array offered{VkSurfaceFormatKHR{VK_FORMAT_UNDEFINED, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR}};
    const auto result = select_surface(capabilities(), offered, modes, {1280, 720});
    REQUIRE(std::holds_alternative<SurfacePlan>(result));
    offered[0].format = VK_FORMAT_R8G8B8A8_UNORM;
    REQUIRE(std::get<SurfacePlan>(result).format.format == VK_FORMAT_B8G8R8A8_SRGB);
}

TEST_CASE("unsupported surface formats and presentation modes fail explicitly") {
    auto offered = formats;
    auto offered_modes = modes;
    SurfaceIssue expected = SurfaceIssue::unsupported_format;
    SECTION("no sRGB color format") {
        offered[0].format = VK_FORMAT_R8G8B8A8_UNORM;
        offered[1].format = VK_FORMAT_B8G8R8A8_UNORM;
    }
    SECTION("no nonlinear sRGB color space") {
        for (auto &format : offered)
            format.colorSpace = VK_COLOR_SPACE_DISPLAY_P3_NONLINEAR_EXT;
    }
    SECTION("no FIFO presentation") {
        offered_modes[1] = VK_PRESENT_MODE_IMMEDIATE_KHR;
        expected = SurfaceIssue::unsupported_present_mode;
    }
    const auto result = select_surface(capabilities(), offered, offered_modes, {1280, 720});
    REQUIRE(std::holds_alternative<SurfaceFailure>(result));
    REQUIRE(std::get<SurfaceFailure>(result).issue == expected);
}

TEST_CASE("surface extent follows fixed size or clamps variable drawable size") {
    auto limits = capabilities();
    SECTION("fixed size overrides the requested drawable") { limits.currentExtent = {320, 240}; }
    SECTION("variable size is clamped to the device bounds") {
        limits.minImageExtent = {1920, 1080};
        limits.maxImageExtent = {1920, 1080};
    }
    SECTION("zero fixed size suspends") {
        limits.currentExtent = {0, 0};
        limits.minImageExtent = {0, 0};
        const auto result = select_surface(limits, formats, modes, {1280, 720});
        REQUIRE(std::holds_alternative<SurfaceFailure>(result));
        REQUIRE(std::get<SurfaceFailure>(result).issue == SurfaceIssue::not_drawable);
        return;
    }
    const auto result = select_surface(limits, formats, modes, {1280, 720});
    REQUIRE(std::holds_alternative<SurfacePlan>(result));
    const auto &plan = std::get<SurfacePlan>(result);
    const auto expected =
        limits.currentExtent.width == UINT32_MAX ? limits.minImageExtent : limits.currentExtent;
    REQUIRE(plan.extent.width == expected.width);
    REQUIRE(plan.extent.height == expected.height);
}

TEST_CASE("surface requires color attachment usage") {
    auto limits = capabilities();
    limits.supportedUsageFlags = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    const auto result = select_surface(limits, formats, modes, {1280, 720});
    REQUIRE(std::holds_alternative<SurfaceFailure>(result));
    REQUIRE(std::get<SurfaceFailure>(result).issue == SurfaceIssue::unsupported_usage);
}

TEST_CASE("surface alpha is selected only from supported modes") {
    for (const auto alpha :
         {VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
          VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR}) {
        auto limits = capabilities();
        limits.supportedCompositeAlpha = alpha;
        const auto result = select_surface(limits, formats, modes, {1280, 720});
        REQUIRE(std::holds_alternative<SurfacePlan>(result));
        REQUIRE(std::get<SurfacePlan>(result).alpha == alpha);
    }
    auto limits = capabilities();
    limits.supportedCompositeAlpha = 0;
    const auto result = select_surface(limits, formats, modes, {1280, 720});
    REQUIRE(std::holds_alternative<SurfaceFailure>(result));
    REQUIRE(std::get<SurfaceFailure>(result).issue == SurfaceIssue::unsupported_alpha);
}

TEST_CASE("image count respects finite limits and cannot wrap") {
    auto limits = capabilities();
    std::uint32_t expected = 2;
    SECTION("no spare image permitted") { limits.maxImageCount = 2; }
    SECTION("minimum count already at the integer limit") {
        limits.minImageCount = UINT32_MAX;
        expected = UINT32_MAX;
    }
    const auto result = select_surface(limits, formats, modes, {1280, 720});
    REQUIRE(std::holds_alternative<SurfacePlan>(result));
    REQUIRE(std::get<SurfacePlan>(result).image_count == expected);
}

TEST_CASE("malformed surface bounds and image counts fail before planning") {
    auto invalid = capabilities();
    SECTION("transform is not a single supported bit") {
        invalid.currentTransform = VK_SURFACE_TRANSFORM_FLAG_BITS_MAX_ENUM_KHR;
    }
    SECTION("unsupported transform") {
        invalid.supportedTransforms = VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR;
    }
    SECTION("zero minimum images") { invalid.minImageCount = 0; }
    SECTION("no image array layer available") { invalid.maxImageArrayLayers = 0; }
    SECTION("maximum images below minimum") { invalid.maxImageCount = 1; }
    SECTION("inverted width bounds") { invalid.minImageExtent.width = 4097; }
    SECTION("inverted height bounds") { invalid.minImageExtent.height = 4097; }
    SECTION("partly undefined current extent") { invalid.currentExtent.height = 720; }
    SECTION("fixed extent outside advertised bounds") { invalid.currentExtent = {8192, 720}; }
    const auto result = select_surface(invalid, formats, modes, {1280, 720});
    REQUIRE(std::holds_alternative<SurfaceFailure>(result));
    REQUIRE(std::get<SurfaceFailure>(result).issue == SurfaceIssue::invalid_capabilities);
}

TEST_CASE("zero drawable extent suspends instead of requesting an invalid swapchain") {
    for (const auto extent : {VkExtent2D{0, 720}, VkExtent2D{1280, 0}, VkExtent2D{0, 0}}) {
        const auto result = select_surface(capabilities(), formats, modes, extent);
        REQUIRE(std::holds_alternative<SurfaceFailure>(result));
        REQUIRE(std::get<SurfaceFailure>(result).issue == SurfaceIssue::not_drawable);
    }
}
