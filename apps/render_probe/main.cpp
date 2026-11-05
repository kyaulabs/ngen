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
    if (check_presentation) {
        unsigned queued = 0;
        unsigned resize_stage = 0;
        const bool check_minimize = std::string_view(SDL_GetCurrentVideoDriver()) == "windows";
        bool minimized_verified = false;
        const auto wait_minimized = [&](bool expected) {
            const auto start_wait = SDL_GetTicks();
            do {
                SDL_PumpEvents();
                if (((SDL_GetWindowFlags(context->window()) & SDL_WINDOW_MINIMIZED) != 0) ==
                    expected)
                    return true;
                SDL_Delay(1);
            } while (SDL_GetTicks() - start_wait < 2000);
            return false;
        };
        std::uint64_t before_resize = 0;
        const auto resized = [&](VkExtent2D expected) {
            int width = 0, height = 0, pixels_wide = 0, pixels_high = 0;
            if (!SDL_GetWindowSize(context->window(), &width, &height) ||
                !SDL_GetWindowSizeInPixels(context->window(), &pixels_wide, &pixels_high))
                return false;
            const auto state = context->presentation_state();
            return state.generation > before_resize &&
                   static_cast<std::uint32_t>(width) == expected.width &&
                   static_cast<std::uint32_t>(height) == expected.height &&
                   state.extent.width == static_cast<std::uint32_t>(pixels_wide) &&
                   state.extent.height == static_cast<std::uint32_t>(pixels_high);
        };
        const auto start = SDL_GetTicks();
        while ((queued < 96 || resize_stage != 3) && SDL_GetTicks() - start < 10000) {
            SDL_Event event{};
            for (unsigned events = 0; events < 256 && SDL_PollEvent(&event); ++events) {
                if (event.type == SDL_EVENT_QUIT)
                    return 1;
            }
            const auto frame = context->render_frame();
            if (const auto *failure = std::get_if<ngen::render::Failure>(&frame)) {
                std::fprintf(stderr, "Vulkan presentation: %s\n", failure->message.c_str());
                return 1;
            }
            if (std::get<ngen::render::FrameOutcome>(frame) == ngen::render::FrameOutcome::queued)
                ++queued;
            if (check_minimize && queued >= 48 && !minimized_verified) {
                if (!SDL_MinimizeWindow(context->window()) || !wait_minimized(true)) {
                    std::fputs("Vulkan probe: minimize was not observed\n", stderr);
                    return 1;
                }
                const auto paused = context->render_frame();
                if (!std::holds_alternative<ngen::render::FrameOutcome>(paused) ||
                    std::get<ngen::render::FrameOutcome>(paused) !=
                        ngen::render::FrameOutcome::skipped) {
                    std::fputs("Vulkan probe: minimized window did not suspend rendering\n",
                               stderr);
                    return 1;
                }
                if (!SDL_RestoreWindow(context->window()) || !wait_minimized(false)) {
                    std::fputs("Vulkan probe: restore was not observed\n", stderr);
                    return 1;
                }
                minimized_verified = true;
            }
            if ((queued >= 32 && resize_stage == 0) ||
                (queued >= 64 && resize_stage == 1 && resized({800, 600}))) {
                before_resize = context->presentation_state().generation;
                const int width = resize_stage == 0 ? 800 : 1280;
                const int height = resize_stage == 0 ? 600 : 720;
                if (!SDL_SetWindowSize(context->window(), width, height)) {
                    std::fprintf(stderr, "Vulkan probe resize: %s\n", SDL_GetError());
                    return 1;
                }
                ++resize_stage;
            }
            if (resize_stage == 2 && resized({1280, 720}))
                resize_stage = 3;
            SDL_Delay(1);
        }
        const auto state = context->presentation_state();
        std::printf("queued_frames=%u swapchain_generations=%llu verified_resizes=%u "
                    "minimize_verified=%u\n",
                    queued, static_cast<unsigned long long>(state.generation),
                    resize_stage == 3 ? 2U : 0U, minimized_verified ? 1U : 0U);
        if (queued < 96 || resize_stage != 3)
            return 1;
    }
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
