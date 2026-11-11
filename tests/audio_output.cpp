#include "audio/output.hpp"
#include <SDL3/SDL.h>
#include <catch2/catch_test_macros.hpp>
#include <string_view>
#include <thread>
#if defined(__linux__)
#include <alsa/asoundlib.h>
#endif

TEST_CASE(
    "audio owner operations reject off-thread and closed access before touching native state") {
    using namespace ngen;
    audio::Output output;
    REQUIRE(std::holds_alternative<std::monostate>(output.update({1, .5F, 0, .5F})));
    std::array<Result<std::monostate>, 6> results;
    std::thread worker([&] {
        SDL_Event event{};
        results = {output.update({}),   output.cue(audio::Cue::confirm),
                   output.mute(true),   output.reset(),
                   output.event(event), output.close()};
    });
    worker.join();
    for (const auto &result : results)
        REQUIRE(std::get<Error>(result) == Error::order);
    REQUIRE(output.statistics().state == audio::State::playing);
    REQUIRE(output.statistics().generation == 1);
    REQUIRE(!output.statistics().muted);
    REQUIRE(std::holds_alternative<std::monostate>(output.close()));
    SDL_Event event{};
    results = {output.update({}),   output.cue(audio::Cue::confirm),
               output.mute(true),   output.reset(),
               output.event(event), output.close()};
    for (std::size_t i = 0; i < 5; ++i)
        REQUIRE(std::get<Error>(results[i]) == Error::closed);
    REQUIRE(std::holds_alternative<std::monostate>(results[5]));
}

TEST_CASE(
    "a missing native audio backend is soft and can reconnect after its configuration recovers") {
    struct Restore {
        ~Restore() { SDL_ResetHint(SDL_HINT_AUDIO_DRIVER); }
    } restore;
    REQUIRE(SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "ngen-unavailable-driver",
                                    SDL_HINT_OVERRIDE));
    ngen::audio::Output output;
    REQUIRE(std::holds_alternative<std::monostate>(output.update({1, 1, 0, .5F})));
    REQUIRE(output.statistics().state == ngen::audio::State::disconnected);
    REQUIRE(output.statistics().generation == 0);
    REQUIRE(output.statistics().message[0] != '\0');
    REQUIRE(SDL_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, "dummy", SDL_HINT_OVERRIDE));
    SDL_Event event{};
    event.type = SDL_EVENT_AUDIO_DEVICE_ADDED;
    REQUIRE(std::holds_alternative<std::monostate>(output.event(event)));
    REQUIRE(std::holds_alternative<std::monostate>(output.update({1, 1, 0, .5F})));
    REQUIRE(output.statistics().state == ngen::audio::State::playing);
    REQUIRE(output.statistics().generation == 1);
    REQUIRE(std::holds_alternative<std::monostate>(output.close()));
}

TEST_CASE(
    "reported playback loss retires its stream and a new device reconnects without stale cues") {
    ngen::audio::Output output;
    REQUIRE(std::holds_alternative<std::monostate>(output.update({1, 1, 0, .5F})));
    SDL_Event event{};
    event.type = SDL_EVENT_AUDIO_DEVICE_REMOVED;
    event.adevice.recording = true;
    REQUIRE(std::holds_alternative<std::monostate>(output.event(event)));
    REQUIRE(output.statistics().state == ngen::audio::State::playing);
    event.adevice.recording = false;
    REQUIRE(std::holds_alternative<std::monostate>(output.event(event)));
    REQUIRE(output.statistics().state == ngen::audio::State::disconnected);
    REQUIRE(output.statistics().queued_frames == 0);
    REQUIRE(output.statistics().losses == 1);
    for (unsigned i = 0; i < 100; ++i)
        REQUIRE(std::holds_alternative<std::monostate>(output.cue(ngen::audio::Cue::confirm)));
    event.type = SDL_EVENT_AUDIO_DEVICE_ADDED;
    REQUIRE(std::holds_alternative<std::monostate>(output.event(event)));
    REQUIRE(std::holds_alternative<std::monostate>(output.update({1, .5F, 0, .5F})));
    REQUIRE(output.statistics().state == ngen::audio::State::playing);
    REQUIRE(output.statistics().generation == 2);
    REQUIRE(output.statistics().queued_frames <= ngen::audio::Output::queue_frame_limit);
    REQUIRE(std::holds_alternative<std::monostate>(output.close()));
}

TEST_CASE("native mute pauses and clears queued PCM while reset preserves the mute preference") {
    ngen::audio::Output output;
    REQUIRE(std::holds_alternative<std::monostate>(output.update({1, 1, 1, 1})));
    REQUIRE(!output.statistics().paused);
    REQUIRE(std::holds_alternative<std::monostate>(output.mute(true)));
    auto stats = output.statistics();
    REQUIRE(stats.paused);
    REQUIRE(stats.queued_frames == 0);
    REQUIRE(stats.muted);
    REQUIRE(stats.state == ngen::audio::State::muted);
    REQUIRE(std::holds_alternative<std::monostate>(output.reset()));
    REQUIRE(std::holds_alternative<std::monostate>(output.update({1, 1, 1, 1})));
    REQUIRE(output.statistics().paused);
    REQUIRE(output.statistics().queued_frames == 0);
    REQUIRE(std::holds_alternative<std::monostate>(output.mute(false)));
    REQUIRE(std::holds_alternative<std::monostate>(output.update({1, .5F, 0, .5F})));
    REQUIRE(!output.statistics().paused);
    REQUIRE(output.statistics().generation == 1);
    REQUIRE(std::holds_alternative<std::monostate>(output.reset()));
    REQUIRE(output.statistics().queued_frames == 0);
    REQUIRE(std::holds_alternative<std::monostate>(output.close()));
}

TEST_CASE("the native audio owner opens a real SDL dummy stream and bounds its copied PCM queue") {
    ngen::audio::Output output;
    for (unsigned i = 0; i < 40; ++i) {
        REQUIRE(std::holds_alternative<std::monostate>(output.update({1, .8F, .2F, .5F})));
        const auto stats = output.statistics();
        REQUIRE(stats.state == ngen::audio::State::playing);
        REQUIRE(stats.generation == 1);
        REQUIRE(stats.queued_frames <= ngen::audio::Output::queue_frame_limit);
        REQUIRE(stats.maximum_queued_frames <= ngen::audio::Output::queue_frame_limit);
    }
    REQUIRE(std::holds_alternative<std::monostate>(output.close()));
    REQUIRE(output.statistics().state == ngen::audio::State::closed);
}

TEST_CASE("the audio SDK contains its declared native backend and no ambient Linux backends") {
#if defined(__linux__)
    // Load the pinned shared client even with the dummy backend. LD_BIND_NOW
    // proves Clang sanitizer imports resolve from the final executable.
    REQUIRE(snd_asoundlib_version() != nullptr);
#endif
    const auto count = SDL_GetNumAudioDrivers();
    REQUIRE(count > 0);
    bool native = false;
    for (int i = 0; i < count; ++i) {
        const auto *name = SDL_GetAudioDriver(i);
        REQUIRE(name != nullptr);
        const std::string_view driver(name);
#if defined(__linux__)
        REQUIRE((driver == "alsa" || driver == "dummy" || driver == "disk"));
        native = native || driver == "alsa";
#else
        native = native || (driver != "dummy" && driver != "disk");
#endif
    }
    REQUIRE(native);
}
