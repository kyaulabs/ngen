#include <SDL3/SDL.h>
#include <dlfcn.h>

extern "C" bool SDLCALL SDL_PollEvent(SDL_Event *event) {
    using Poll = bool(SDLCALL *)(SDL_Event *);
    const auto real = reinterpret_cast<Poll>(dlsym(RTLD_NEXT, "SDL_PollEvent"));
    if (real(event))
        return true;
    if (event)
        event->type = SDL_EVENT_USER;
    return true;
}
