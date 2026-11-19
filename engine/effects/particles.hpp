#pragma once
#include "world/coordinates/position.hpp"
#include <array>
#include <span>

namespace ngen::effects {
struct Particle {
    std::array<double, 3> position_metres{}, velocity_mps{};
    unsigned age_ticks{}, lifetime_ticks{};
};
struct ParticleSnapshot {
    static constexpr std::size_t capacity = 256;
    coordinates::Frame frame;

    [[nodiscard]] std::span<const Particle> particles() const & {
        return std::span(storage_).first(count_);
    }
    std::span<const Particle> particles() const && = delete;

  private:
    friend class Particles;
    std::array<Particle, capacity> storage_{};
    std::size_t count_{};
};

// Called by one simulation owner at the 60 Hz tick boundary. No renderer or
// authoritative collision state is owned here. Snapshots own their particles.
class Particles {
  public:
    static constexpr unsigned max_lifetime_ticks = 240;
    [[nodiscard]] static Result<Particles> create(const coordinates::Frame &frame);
    [[nodiscard]] Result<std::monostate> emit(const coordinates::Sample &sample,
                                              unsigned lifetime_ticks);
    [[nodiscard]] Result<Tick> step();
    [[nodiscard]] Result<std::monostate> rebase(const coordinates::Frame &frame);
    [[nodiscard]] ParticleSnapshot snapshot() const { return state_; }

  private:
    explicit Particles(const coordinates::Frame &frame) { state_.frame = frame; }
    ParticleSnapshot state_;
};
} // namespace ngen::effects
