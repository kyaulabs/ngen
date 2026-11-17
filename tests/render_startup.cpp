#include "render/context.hpp"
#include <catch2/catch_test_macros.hpp>
#include <type_traits>

// WSI must not accept arbitrary unresolved external timeline dependencies.
static_assert(!std::is_constructible_v<ngen::render::ImageFrame, const ngen::render::Image &, float,
                                       bool, ngen::render::TimelineWait>);

TEST_CASE("unavailable video driver fails explicitly and releases SDL state") {
    REQUIRE(SDL_SetHint(SDL_HINT_VIDEO_DRIVER, "ngen_missing_driver"));
    SDL_SetError("stale error must not escape");
    const auto result = ngen::render::Context::create({false, false});
    REQUIRE(std::holds_alternative<ngen::render::Failure>(result));
    REQUIRE(std::get<ngen::render::Failure>(result).message.find("ngen_missing_driver") !=
            std::string::npos);
    REQUIRE(SDL_WasInit(SDL_INIT_VIDEO) == 0);
    SDL_ResetHint(SDL_HINT_VIDEO_DRIVER);
}
