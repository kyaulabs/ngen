// Observation only: record visibility/focus and reported display refresh.
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>
#include <dlfcn.h>
#include <sys/prctl.h>
#include <time.h>
extern "C" SDL_WindowFlags SDLCALL SDL_GetWindowFlags(SDL_Window *window) {
    static auto next = reinterpret_cast<decltype(&SDL_GetWindowFlags)>(dlsym(RTLD_NEXT, "SDL_GetWindowFlags"));
    if (!next)
        std::abort();
    const auto flags = next(window);
    static thread_local SDL_Window *previous{};
    static thread_local SDL_WindowFlags previous_flags{};
    if (window != previous || flags != previous_flags) {
        const auto display = SDL_GetDisplayForWindow(window);
        const auto *mode = SDL_GetCurrentDisplayMode(display);
        std::fprintf(stderr, "window_state flags=%llu hidden=%d minimized=%d occluded=%d focus=%d refresh_hz=%.3f\n",
            static_cast<unsigned long long>(flags), (flags & SDL_WINDOW_HIDDEN) != 0,
            (flags & SDL_WINDOW_MINIMIZED) != 0, (flags & SDL_WINDOW_OCCLUDED) != 0,
            (flags & SDL_WINDOW_INPUT_FOCUS) != 0, mode ? static_cast<double>(mode->refresh_rate) : 0.0);
        std::fprintf(stderr, "window_thread_timer_slack_ns=%d\n", prctl(PR_GET_TIMERSLACK));
        previous = window;
        previous_flags = flags;
    }
    return flags;
}
extern "C" void SDLCALL SDL_Delay(Uint32 milliseconds) {
    static auto next = reinterpret_cast<decltype(&SDL_Delay)>(dlsym(RTLD_NEXT, "SDL_Delay"));
    if (!next)
        std::abort();
    timespec begin{}, end{};
    (void)clock_gettime(CLOCK_MONOTONIC, &begin);
    next(milliseconds);
    (void)clock_gettime(CLOCK_MONOTONIC, &end);
    const auto duration = (end.tv_sec - begin.tv_sec) * 1000000000LL + end.tv_nsec - begin.tv_nsec;
    static thread_local unsigned reports{};
    if (milliseconds == 1 && reports < 12) {
        std::fprintf(stderr, "delay_observation requested_ms=%u actual_ns=%lld slack_ns=%d\n", milliseconds, duration, prctl(PR_GET_TIMERSLACK));
        ++reports;
    }
}
