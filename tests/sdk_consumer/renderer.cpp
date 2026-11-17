#include "render/context.hpp"

#include <cstdio>
#include <exception>

int main(int argc, char **) try {
    using namespace ngen::render;
    const VkSurfaceCapabilitiesKHR capabilities{
        .minImageCount = 1,
        .maxImageCount = 2,
        .currentExtent = {640, 480},
        .minImageExtent = {1, 1},
        .maxImageExtent = {640, 480},
        .maxImageArrayLayers = 1,
        .supportedTransforms = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .currentTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
        .supportedCompositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .supportedUsageFlags = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};
    const auto plan = select_surface(capabilities, {}, {}, {});
    const auto *failure = std::get_if<SurfaceFailure>(&plan);
    if (!failure || failure->issue != SurfaceIssue::not_drawable || SDL_GetVersion() < 3004000)
        return 1;
    if (argc == 1)
        return 0;

    const auto startup =
        Context::create({.validation = true, .visible = true, .presentation = true});
    if (const auto *error = std::get_if<Failure>(&startup)) {
        std::fprintf(stderr, "%s\n", error->message.c_str());
        return 2;
    }
    const auto &context = std::get<std::unique_ptr<Context>>(startup);
    const auto frame = context->render_frame();
    const bool closed = context->close();
    if (const auto *error = std::get_if<Failure>(&frame))
        std::fprintf(stderr, "%s\n", error->message.c_str());
    const auto *outcome = std::get_if<FrameOutcome>(&frame);
    return closed && outcome && *outcome == FrameOutcome::queued &&
                   context->validation_errors() == 0
               ? 0
               : 3;
} catch (const std::exception &error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 4;
} catch (...) {
    std::fputs("Unexpected renderer consumer failure\n", stderr);
    return 4;
}
