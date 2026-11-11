#pragma once
#include "core/values.hpp"
#include <array>
#include <span>

namespace ngen::audio {
struct Levels {
    float engine{}, thrust{}, brake{}, speed{}; // Normalized, finite [0,1].
};
enum class Cue : std::uint8_t { confirm, reset, impact, count };
// Owner-thread value mixer; no device, callback, heap or gameplay dependency.
// Original procedural stereo PCM at48kHz. Mute/reset discard old transient cues.
class Mixer {
  public:
    static constexpr unsigned sample_rate = 48000, channels = 2;
    static constexpr std::size_t voice_capacity = 8, frame_capacity = 2048;
    [[nodiscard]] Result<std::monostate> levels(Levels value);
    [[nodiscard]] Result<std::monostate> cue(Cue value);
    [[nodiscard]] Result<std::monostate> render(std::span<float> interleaved_stereo);
    void mute(bool value);
    void reset(); // Preserve the user's mute preference.
    [[nodiscard]] bool muted() const { return muted_; }

  private:
    struct Voice {
        double phase{};
        std::uint32_t remaining{}, duration{};
        Cue kind{Cue::confirm};
    };
    Levels target_, current_;
    std::array<Voice, voice_capacity> voices_{};
    std::array<double, 3> phase_{};
    std::uint32_t noise_{0x4e47454eU};
    float filtered_noise_{};
    bool muted_{};
};
} // namespace ngen::audio
