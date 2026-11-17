#include "probe/presentation.hpp"
#include "render/context.hpp"
#include <cstdio>
#include <string_view>

namespace ngen::probe {
bool presentation(render::Context &context, const std::function<bool()> &step,
                  const render::Image *source) {
    if (!SDL_ShowWindow(context.window())) {
        std::fprintf(stderr, "Vulkan probe show window: %s\n", SDL_GetError());
        return false;
    }
    unsigned queued = 0;
    unsigned resize_stage = 0;
    const bool check_minimize = std::string_view(SDL_GetCurrentVideoDriver()) == "windows";
    bool minimized_verified = false;
    const auto wait_minimized = [&](bool expected) {
        const auto start_wait = SDL_GetTicks();
        do {
            SDL_PumpEvents();
            if (((SDL_GetWindowFlags(context.window()) & SDL_WINDOW_MINIMIZED) != 0) == expected)
                return true;
            SDL_Delay(1);
        } while (SDL_GetTicks() - start_wait < 2000);
        return false;
    };
    std::uint64_t before_resize = 0;
    const auto resized = [&](VkExtent2D expected) {
        int width = 0, height = 0, pixels_wide = 0, pixels_high = 0;
        if (!SDL_GetWindowSize(context.window(), &width, &height) ||
            !SDL_GetWindowSizeInPixels(context.window(), &pixels_wide, &pixels_high))
            return false;
        const auto state = context.presentation_state();
        return state.generation > before_resize &&
               static_cast<std::uint32_t>(width) == expected.width &&
               static_cast<std::uint32_t>(height) == expected.height &&
               state.extent.width == static_cast<std::uint32_t>(pixels_wide) &&
               state.extent.height == static_cast<std::uint32_t>(pixels_high);
    };
    const auto draw = [&]() -> std::variant<render::FrameOutcome, render::Failure> {
        if (!source)
            return context.render_frame();
        const auto frame = context.render_frame({*source, 1, false});
        if (const auto *failure = std::get_if<render::Failure>(&frame))
            return *failure;
        return std::get<render::FrameResult>(frame).outcome;
    };
    const auto start = SDL_GetTicks();
    while ((queued < 96 || resize_stage != 3) && SDL_GetTicks() - start < 10000) {
        SDL_Event event{};
        for (unsigned events = 0; events < 256 && SDL_PollEvent(&event); ++events) {
            if (event.type == SDL_EVENT_QUIT)
                return false;
        }
        if (step && !step())
            return false;
        const auto frame = draw();
        if (const auto *failure = std::get_if<render::Failure>(&frame)) {
            std::fprintf(stderr, "Vulkan presentation: %s\n", failure->message.c_str());
            return false;
        }
        if (std::get<render::FrameOutcome>(frame) == render::FrameOutcome::queued)
            ++queued;
        if (check_minimize && queued >= 48 && !minimized_verified) {
            if (!SDL_MinimizeWindow(context.window()) || !wait_minimized(true)) {
                std::fputs("Vulkan probe: minimize was not observed\n", stderr);
                return false;
            }
            const auto paused = draw();
            if (!std::holds_alternative<render::FrameOutcome>(paused) ||
                std::get<render::FrameOutcome>(paused) != render::FrameOutcome::skipped) {
                std::fputs("Vulkan probe: minimized window did not suspend rendering\n", stderr);
                return false;
            }
            if (!SDL_RestoreWindow(context.window()) || !wait_minimized(false)) {
                std::fputs("Vulkan probe: restore was not observed\n", stderr);
                return false;
            }
            minimized_verified = true;
        }
        if ((queued >= 32 && resize_stage == 0) ||
            (queued >= 64 && resize_stage == 1 && resized({800, 600}))) {
            before_resize = context.presentation_state().generation;
            const int width = resize_stage == 0 ? 800 : 1280;
            const int height = resize_stage == 0 ? 600 : 720;
            if (!SDL_SetWindowSize(context.window(), width, height)) {
                std::fprintf(stderr, "Vulkan probe resize: %s\n", SDL_GetError());
                return false;
            }
            ++resize_stage;
        }
        if (resize_stage == 2 && resized({1280, 720}))
            resize_stage = 3;
        SDL_Delay(1);
    }
    const auto state = context.presentation_state();
    std::printf("queued_frames=%u swapchain_generations=%llu verified_resizes=%u "
                "minimize_verified=%u sampled_source=%u\n",
                queued, static_cast<unsigned long long>(state.generation),
                resize_stage == 3 ? 2U : 0U, minimized_verified ? 1U : 0U, source ? 1U : 0U);
    return queued >= 96 && resize_stage == 3;
}
} // namespace ngen::probe
