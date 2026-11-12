#include "audio/output.hpp"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cstdio>

namespace ngen::audio {
Output::Output() : owner_(std::this_thread::get_id()) {}
Output::~Output() { release(); }
Statistics Output::statistics() const {
    auto value = statistics_;
    if (stream_) {
        value.paused = SDL_AudioStreamDevicePaused(stream_);
        const auto bytes = SDL_GetAudioStreamQueued(stream_);
        if (bytes >= 0)
            value.queued_frames = static_cast<std::uint32_t>(
                bytes / static_cast<int>(Mixer::channels * sizeof(float)));
    }
    return value;
}
void Output::release() {
    if (stream_)
        SDL_DestroyAudioStream(stream_);
    stream_ = nullptr;
    if (initialized_)
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
    initialized_ = false;
    started_ = false;
    statistics_.queued_frames = 0;
}
void Output::disconnect(const char *message) {
    std::snprintf(statistics_.message.data(), statistics_.message.size(), "%s: %s", message,
                  SDL_GetError());
    if (stream_) {
        SDL_DestroyAudioStream(stream_);
        ++statistics_.losses;
    }
    stream_ = nullptr;
    started_ = false;
    mixer_.reset();
    statistics_.queued_frames = 0;
    statistics_.state = State::disconnected;
    retry_ns_ = SDL_GetTicksNS() + 1000000000ULL;
}
bool Output::prepare() {
    if (stream_)
        return true;
    if (SDL_GetTicksNS() < retry_ns_)
        return false;
    if (!initialized_) {
        if (!SDL_InitSubSystem(SDL_INIT_AUDIO)) {
            disconnect("audio initialization");
            return false;
        }
        initialized_ = true;
    }
    SDL_AudioSpec spec{};
    spec.format = SDL_AUDIO_F32;
    spec.channels = static_cast<int>(Mixer::channels);
    spec.freq = static_cast<int>(Mixer::sample_rate);
    stream_ = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (!stream_) {
        disconnect("audio device open");
        return false;
    }
    ++statistics_.generation;
    statistics_.message.fill(0);
    return true;
}
Result<std::monostate> Output::update(Levels value) {
    if (std::this_thread::get_id() != owner_)
        return Error::order;
    if (closed_)
        return Error::closed;
    const auto admitted = mixer_.levels(value);
    if (std::holds_alternative<Error>(admitted))
        return std::get<Error>(admitted);
    if (!prepare())
        return std::monostate{};
    statistics_.muted = mixer_.muted();
    if (statistics_.muted) {
        statistics_.state = State::muted;
        return std::monostate{};
    }
    const auto bytes = SDL_GetAudioStreamQueued(stream_);
    if (bytes < 0) {
        disconnect("audio queue query");
        return std::monostate{};
    }
    auto frames =
        static_cast<std::uint32_t>(bytes / static_cast<int>(Mixer::channels * sizeof(float)));
    for (unsigned block = 0; block < 8 && frames < queue_frame_limit; ++block) {
        const auto count = std::min(std::uint32_t{512}, queue_frame_limit - frames);
        auto samples = std::span(pcm_).first(std::size_t{count} * Mixer::channels);
        const auto rendered = mixer_.render(samples);
        if (std::holds_alternative<Error>(rendered))
            return std::get<Error>(rendered);
        if (!SDL_PutAudioStreamData(stream_, samples.data(),
                                    static_cast<int>(samples.size_bytes()))) {
            disconnect("audio queue write");
            return std::monostate{};
        }
        frames += count;
    }
    statistics_.maximum_queued_frames = std::max(statistics_.maximum_queued_frames, frames);
    statistics_.queued_frames = frames;
    if (!started_) {
        if (!SDL_ResumeAudioStreamDevice(stream_)) {
            disconnect("audio device resume");
            return std::monostate{};
        }
        started_ = true;
    }
    statistics_.state = State::playing;
    return std::monostate{};
}
Result<std::monostate> Output::cue(Cue value) {
    if (std::this_thread::get_id() != owner_)
        return Error::order;
    if (closed_)
        return Error::closed;
    if (value >= Cue::count)
        return Error::invalid;
    if (!stream_)
        return std::monostate{}; // Never replay stale UI cues on reconnect.
    return mixer_.cue(value);
}
Result<std::monostate> Output::mute(bool value) {
    if (std::this_thread::get_id() != owner_)
        return Error::order;
    if (closed_)
        return Error::closed;
    if (mixer_.muted() == value)
        return std::monostate{};
    mixer_.mute(value);
    statistics_.muted = value;
    if (stream_) {
        if (!SDL_PauseAudioStreamDevice(stream_) || !SDL_ClearAudioStream(stream_)) {
            disconnect("audio mute clear");
            return std::monostate{};
        }
        started_ = false;
        statistics_.queued_frames = 0;
        if (!value) {
            if (!SDL_ResumeAudioStreamDevice(stream_)) {
                disconnect("audio unmute resume");
                return std::monostate{};
            }
            started_ = true;
        }
        statistics_.state = value ? State::muted : State::playing;
    }
    return std::monostate{};
}
Result<std::monostate> Output::reset() {
    if (std::this_thread::get_id() != owner_)
        return Error::order;
    if (closed_)
        return Error::closed;
    mixer_.reset();
    if (stream_ && !SDL_ClearAudioStream(stream_))
        disconnect("audio reset clear");
    statistics_.queued_frames = 0;
    return std::monostate{};
}
Result<std::monostate> Output::event(const SDL_Event &event) {
    if (std::this_thread::get_id() != owner_)
        return Error::order;
    if (closed_)
        return Error::closed;
    if (event.type != SDL_EVENT_AUDIO_DEVICE_ADDED && event.type != SDL_EVENT_AUDIO_DEVICE_REMOVED)
        return std::monostate{};
    if (event.adevice.recording)
        return std::monostate{};
    if (event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED && stream_) {
        SDL_ClearError();
        disconnect("reported playback device removal");
    } else if (event.type == SDL_EVENT_AUDIO_DEVICE_ADDED && !stream_) {
        retry_ns_ = 0;
    }
    return std::monostate{};
}
Result<std::monostate> Output::close() {
    if (std::this_thread::get_id() != owner_)
        return Error::order;
    release();
    mixer_.reset();
    closed_ = true;
    statistics_.state = State::closed;
    return std::monostate{};
}
} // namespace ngen::audio
