#include "audio/mixer.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
namespace ngen::audio {
Result<std::monostate> Mixer::levels(Levels value) {
    const std::array values{value.engine, value.thrust, value.brake, value.speed};
    if (!std::all_of(values.begin(), values.end(),
                     [](float v) { return std::isfinite(v) && v >= 0 && v <= 1; }))
        return Error::invalid;
    target_ = value;
    return std::monostate{};
}
Result<std::monostate> Mixer::cue(Cue value) {
    if (value >= Cue::count)
        return Error::invalid;
    if (muted_)
        return std::monostate{};
    const auto slot = std::find_if(voices_.begin(), voices_.end(),
                                   [](const Voice &voice) { return voice.remaining == 0; });
    if (slot == voices_.end())
        return Error::capacity;
    constexpr std::array<std::uint32_t, 3> duration{2400, 4800, 7200};
    const auto length = duration[static_cast<std::size_t>(value)];
    *slot = {0, length, length, value};
    return std::monostate{};
}
Result<std::monostate> Mixer::render(std::span<float> output) {
    if (output.empty() || output.size() % channels != 0)
        return Error::invalid;
    if (output.size() / channels > frame_capacity)
        return Error::capacity;
    if (muted_) {
        std::fill(output.begin(), output.end(), 0);
        return std::monostate{};
    }
    const auto ramp = [](float &value, float target) {
        value += std::clamp(target - value, -1.0F / 960, 1.0F / 960);
    };
    const auto wave = [&](std::size_t index, float frequency) {
        phase_[index] += static_cast<double>(frequency) / sample_rate;
        if (phase_[index] >= 1)
            phase_[index] -= 1;
        return static_cast<float>(std::sin(phase_[index] * 2 * std::numbers::pi));
    };
    for (std::size_t i = 0; i < output.size(); i += channels) {
        ramp(current_.engine, target_.engine);
        ramp(current_.thrust, target_.thrust);
        ramp(current_.brake, target_.brake);
        ramp(current_.speed, target_.speed);
        noise_ ^= noise_ << 13U;
        noise_ ^= noise_ >> 17U;
        noise_ ^= noise_ << 5U;
        const auto noise = static_cast<float>(noise_ & 0x00ffffffU) / 16777215.0F * 2 - 1;
        filtered_noise_ += .08F * (noise - filtered_noise_);
        const auto engine = wave(0, 43 + 90 * current_.speed);
        const auto thrust = wave(1, 90 + 120 * current_.speed);
        const auto brake = wave(2, 310 + 120 * current_.speed);
        auto value = .026F * current_.engine * engine +
                     .065F * current_.thrust * (.7F * filtered_noise_ + .3F * thrust) +
                     .04F * current_.brake * (.6F * filtered_noise_ + .4F * brake);
        for (auto &voice : voices_) {
            if (voice.remaining == 0)
                continue;
            const auto fraction =
                static_cast<float>(voice.remaining) / static_cast<float>(voice.duration);
            const auto attack =
                std::min(1.0F, static_cast<float>(voice.duration - voice.remaining) / 96);
            const auto frequency =
                voice.kind == Cue::confirm
                    ? 880.0F
                    : (voice.kind == Cue::reset ? 330 + 330 * fraction : 55 + 90 * fraction);
            voice.phase += static_cast<double>(frequency) / sample_rate;
            if (voice.phase >= 1)
                voice.phase -= 1;
            const auto tone = static_cast<float>(std::sin(voice.phase * 2 * std::numbers::pi));
            const auto signal = voice.kind == Cue::impact ? (tone + noise) * .5F : tone;
            value += signal * .06F * fraction * attack;
            --voice.remaining;
        }
        output[i] = output[i + 1] = std::clamp(value, -.8F, .8F);
    }
    return std::monostate{};
}
void Mixer::mute(bool value) {
    if (value && !muted_) {
        voices_ = {};
        current_ = {};
    }
    muted_ = value;
}
void Mixer::reset() {
    target_ = {};
    current_ = {};
    voices_ = {};
    phase_ = {};
    filtered_noise_ = 0;
    noise_ = 0x4e47454eU;
}
} // namespace ngen::audio
