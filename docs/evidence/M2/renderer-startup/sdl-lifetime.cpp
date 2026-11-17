#include <SDL3/SDL.h>
#include <cstdio>
int main() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        std::fprintf(stderr, "%s\n", SDL_GetError());
        return 1;
    }
    SDL_Quit();
    return 0;
}
