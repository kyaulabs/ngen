#include "audio/mixer.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <cmath>
#include <limits>
#include <numeric>

using namespace ngen;

TEST_CASE("audio PCM depends on sample count rather than caller block cadence") {
    audio::Mixer whole;
    REQUIRE(std::holds_alternative<std::monostate>(whole.levels({1, .7F, .2F, .8F})));
    REQUIRE(std::holds_alternative<std::monostate>(whole.cue(audio::Cue::reset)));
    auto split = whole;
    std::array<float, 4096> expected{}, actual{};
    REQUIRE(std::holds_alternative<std::monostate>(whole.render(expected)));
    REQUIRE(std::holds_alternative<std::monostate>(split.render(std::span(actual).first(512))));
    REQUIRE(
        std::holds_alternative<std::monostate>(split.render(std::span(actual).subspan(512, 1536))));
    REQUIRE(std::holds_alternative<std::monostate>(split.render(std::span(actual).subspan(2048))));
    REQUIRE(actual == expected);
}

TEST_CASE("invalid levels, cues and PCM spans neither mutate synthesis nor overwrite caller data") {
    audio::Mixer mixer;
    REQUIRE(std::holds_alternative<std::monostate>(mixer.levels({.5F, .4F, .3F, .2F})));
    const std::array<audio::Levels, 4> invalid{{{std::numeric_limits<float>::quiet_NaN(), 0, 0, 0},
                                                {0, -.1F, 0, 0},
                                                {0, 0, 1.1F, 0},
                                                {0, 0, 0, std::numeric_limits<float>::infinity()}}};
    for (const auto &value : invalid) {
        auto untouched = mixer;
        const auto rejected = mixer.levels(value);
        REQUIRE(std::holds_alternative<Error>(rejected));
        REQUIRE(std::get<Error>(rejected) == Error::invalid);
        std::array<float, 256> actual{}, expected{};
        REQUIRE(std::holds_alternative<std::monostate>(mixer.render(actual)));
        REQUIRE(std::holds_alternative<std::monostate>(untouched.render(expected)));
        REQUIRE(actual == expected);
    }
    auto untouched = mixer;
    REQUIRE(std::get<Error>(mixer.cue(audio::Cue::count)) == Error::invalid);
    std::array<float, audio::Mixer::frame_capacity * 2 + 2> oversized{};
    oversized.fill(42);
    REQUIRE(std::get<Error>(mixer.render(std::span(oversized).first(3))) == Error::invalid);
    REQUIRE(std::get<Error>(mixer.render(oversized)) == Error::capacity);
    REQUIRE(
        std::all_of(oversized.begin(), oversized.end(), [](float value) { return value == 42; }));
    std::array<float, 256> actual{}, expected{};
    REQUIRE(std::holds_alternative<std::monostate>(mixer.render(actual)));
    REQUIRE(std::holds_alternative<std::monostate>(untouched.render(expected)));
    REQUIRE(actual == expected);
}

TEST_CASE("mute and reset silence stale cues without losing the user's mute preference") {
    audio::Mixer mixer;
    std::array<float, 2048> pcm{};
    REQUIRE(std::holds_alternative<std::monostate>(mixer.levels({1, 1, 1, 1})));
    REQUIRE(std::holds_alternative<std::monostate>(mixer.cue(audio::Cue::impact)));
    REQUIRE(std::holds_alternative<std::monostate>(mixer.render(pcm)));
    mixer.mute(true);
    REQUIRE(mixer.muted());
    REQUIRE(std::holds_alternative<std::monostate>(mixer.render(pcm)));
    REQUIRE(std::all_of(pcm.begin(), pcm.end(), [](float value) { return value == 0; }));
    REQUIRE(std::holds_alternative<std::monostate>(mixer.levels({})));
    REQUIRE(std::holds_alternative<std::monostate>(mixer.cue(audio::Cue::reset)));
    mixer.mute(false);
    REQUIRE(std::holds_alternative<std::monostate>(mixer.render(pcm)));
    REQUIRE(std::all_of(pcm.begin(), pcm.end(), [](float value) { return value == 0; }));
    REQUIRE(std::holds_alternative<std::monostate>(mixer.cue(audio::Cue::confirm)));
    mixer.mute(true);
    mixer.reset();
    REQUIRE(mixer.muted());
    mixer.mute(false);
    REQUIRE(std::holds_alternative<std::monostate>(mixer.render(pcm)));
    REQUIRE(std::all_of(pcm.begin(), pcm.end(), [](float value) { return value == 0; }));
}
TEST_CASE("an admitted running drive produces bounded original stereo PCM instead of silence") {
    audio::Mixer mixer;
    std::array<float, 2048> pcm{};
    REQUIRE(std::holds_alternative<std::monostate>(mixer.render(pcm)));
    REQUIRE(std::all_of(pcm.begin(), pcm.end(), [](float value) { return value == 0; }));
    REQUIRE(std::holds_alternative<std::monostate>(mixer.levels({1, 1, .5F, .5F})));
    REQUIRE(std::holds_alternative<std::monostate>(mixer.render(pcm)));
    REQUIRE(std::all_of(pcm.begin(), pcm.end(), [](float value) {
        return std::isfinite(value) && std::abs(value) <= .8F;
    }));
    const auto energy = std::accumulate(
        pcm.begin(), pcm.end(), 0.0, [](double sum, float value) { return sum + std::abs(value); });
    REQUIRE(energy > 1);
}

TEST_CASE("bounded procedural cues are audible, drain, and reject excess voices") {
    audio::Mixer mixer;
    std::array<float, 2048> pcm{};
    REQUIRE(std::holds_alternative<std::monostate>(mixer.cue(audio::Cue::confirm)));
    REQUIRE(std::holds_alternative<std::monostate>(mixer.render(pcm)));
    REQUIRE(
        std::any_of(pcm.begin(), pcm.end(), [](float value) { return std::abs(value) > .01F; }));
    for (unsigned block = 0; block < 10; ++block)
        REQUIRE(std::holds_alternative<std::monostate>(mixer.render(pcm)));
    REQUIRE(std::all_of(pcm.begin(), pcm.end(), [](float value) { return value == 0; }));
    for (std::size_t i = 0; i < audio::Mixer::voice_capacity; ++i)
        REQUIRE(std::holds_alternative<std::monostate>(mixer.cue(audio::Cue::impact)));
    REQUIRE(std::get<Error>(mixer.cue(audio::Cue::reset)) == Error::capacity);
    REQUIRE(std::holds_alternative<std::monostate>(mixer.render(pcm)));
    REQUIRE(std::all_of(pcm.begin(), pcm.end(), [](float value) {
        return std::isfinite(value) && std::abs(value) <= .8F;
    }));
}
