#include "probe/presentation.hpp"
#include "render/context.hpp"
#include <cstdio>
#include <string_view>

int main(int argc, char **argv) try {
    ngen::render::Context::Options options;
    const bool check_presentation =
        argc == 2 && std::string_view(argv[1]) == "--check-presentation";
    options.visible = check_presentation;
    options.presentation = check_presentation;
    const bool check_callback =
        argc == 2 && std::string_view(argv[1]) == "--check-validation-callback";
    if (argc == 2 && std::string_view(argv[1]) == "--without-validation")
        options.validation = false;
    else if (argc != 1 && !check_callback && !check_presentation) {
        std::fputs("Usage: render_probe "
                   "[--without-validation|--check-validation-callback|--check-presentation]\n",
                   stderr);
        return 2;
    }
    auto result = ngen::render::Context::create(options);
    if (const auto *failure = std::get_if<ngen::render::Failure>(&result)) {
        std::fprintf(stderr, "Vulkan startup: %s\n", failure->message.c_str());
        return 1;
    }
    const auto &context = std::get<std::unique_ptr<ngen::render::Context>>(result);
    const auto selection = context->inspect_surface();
    if (const auto *failure = std::get_if<ngen::render::SurfaceFailure>(&selection)) {
        std::fprintf(stderr, "Vulkan surface: %s\n", failure->message.c_str());
        return 1;
    }
    const auto &plan = std::get<ngen::render::SurfacePlan>(selection);
    std::printf("surface=%ux%u format=%d color_space=%d present_mode=%d images=%u\n",
                plan.extent.width, plan.extent.height, plan.format.format, plan.format.colorSpace,
                plan.present_mode, plan.image_count);
    if (check_callback) {
        if (context->validation_errors() != 0)
            return 1;
        const auto submit = reinterpret_cast<PFN_vkSubmitDebugUtilsMessageEXT>(
            vkGetInstanceProcAddr(context->instance(), "vkSubmitDebugUtilsMessageEXT"));
        if (!submit) {
            std::fputs("Vulkan validation message entry point unavailable\n", stderr);
            return 1;
        }
        VkDebugUtilsMessengerCallbackDataEXT message{};
        message.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CALLBACK_DATA_EXT;
        message.pMessageIdName = "NGEN_VALIDATION_SELF_TEST";
        message.pMessage = "expected NGEN validation callback self-test";
        submit(context->instance(), VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
               VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT, &message);
    }
    if (check_presentation && !ngen::probe::presentation(*context))
        return 1;
    const bool closed = context->close();
    const auto closed_surface = context->inspect_surface();
    const auto *closed_failure = std::get_if<ngen::render::SurfaceFailure>(&closed_surface);
    if (!closed_failure || closed_failure->issue != ngen::render::SurfaceIssue::closed ||
        context->presentation_state().generation != 0 ||
        !std::holds_alternative<ngen::render::Failure>(context->render_frame()))
        return 1;
    return closed == !check_callback && context->close() == !check_callback &&
                   context->close_result() == VK_SUCCESS && context->device() == VK_NULL_HANDLE &&
                   context->physical_device() == VK_NULL_HANDLE &&
                   context->queue() == VK_NULL_HANDLE && context->surface() == VK_NULL_HANDLE &&
                   context->instance() == VK_NULL_HANDLE && context->window() == nullptr &&
                   context->queue_family() == UINT32_MAX &&
                   context->validation_errors() == (check_callback ? 1U : 0U)
               ? 0
               : 1;
} catch (const std::exception &error) {
    std::fprintf(stderr, "Vulkan probe: %s\n", error.what());
    return 1;
}
