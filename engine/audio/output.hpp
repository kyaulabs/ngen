#pragma once
#include "audio/mixer.hpp"
#include <thread>

struct SDL_AudioStream;
union SDL_Event;
namespace ngen::audio {
enum class State : std::uint8_t { disconnected, playing, muted, closed };
struct Statistics {
    std::uint64_t generation{}, losses{};
    std::uint32_t queued_frames{}, maximum_queued_frames{};
    State state{State::disconnected};
    bool muted{}, paused{};
    std::array<char, 160> message{};
};
// Owner-thread lifetime/access, nonmovable. SDL owns conversion/playback threads;
// they receive copied PCM only, never this mixer or gameplay state.
class Output {
  public:
    static constexpr std::uint32_t queue_frame_limit = 4096;
    Output();
    ~Output();
    Output(const Output &) = delete;
    Output &operator=(const Output &) = delete;
    // Device absence is a soft disconnected state; retries are rate limited.
    [[nodiscard]] Result<std::monostate> update(Levels levels);
    [[nodiscard]] Result<std::monostate> cue(Cue value);
    [[nodiscard]] Result<std::monostate> mute(bool value);
    [[nodiscard]] Result<std::monostate> reset();
    [[nodiscard]] Result<std::monostate> event(const SDL_Event &event);
    [[nodiscard]] Result<std::monostate> close();
    [[nodiscard]] Statistics statistics() const; // Owner only; copy may cross.

  private:
    const std::thread::id owner_;
    SDL_AudioStream *stream_{};
    Mixer mixer_;
    Statistics statistics_;
    std::array<float, std::size_t{512} * Mixer::channels> pcm_{};
    std::uint64_t retry_ns_{};
    bool initialized_{}, started_{}, closed_{};
    bool prepare();
    void disconnect(const char *message);
    void release();
};
} // namespace ngen::audio
