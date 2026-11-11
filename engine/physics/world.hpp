#pragma once
#include "core/values.hpp"
#include <array>
#include <cstddef>
#include <memory>

namespace ngen::physics {
struct BodyId {
    std::uint32_t slot{};
    std::uint64_t generation{}, world{};
    auto operator<=>(const BodyId &) const = default;
};
struct Box {
    std::array<float, 3> half_extent_metres{1, 1, 1};
    std::array<double, 3> position_metres{};
    double mass_kg{1000};
    bool dynamic{true};
};
struct BodyState {
    BodyId id;
    Tick tick;
    std::array<double, 3> position_metres{};
    std::array<float, 4> rotation_xyzw{0, 0, 0, 1};
    std::array<float, 3> velocity_mps{}, angular_velocity_rps{};
    double mass_kg{};
};
// One owner thread, bounded CPU-only local contact domain. Returned states own
// their values; native handles and mutable body storage never escape.
class World {
  public:
    static constexpr std::size_t body_capacity = 128;
    static constexpr unsigned ticks_per_second = 60;
    [[nodiscard]] static Result<std::unique_ptr<World>> create();
    ~World();
    World(const World &) = delete;
    World &operator=(const World &) = delete;
    [[nodiscard]] Result<BodyId> add_box(const Box &description);
    // Accumulate a next-step wrench. Reject commands that would hit native
    // speed/spin clamps, rather than silently discarding accepted impulse.
    [[nodiscard]] Result<std::monostate> apply(BodyId id, std::array<float, 3> force_newtons,
                                               std::array<float, 3> torque_newton_metres = {});
    // Co-moving payload changes mass/inertia without an impulse. Existing
    // pending forces must still fit the native motion envelope at the new mass.
    [[nodiscard]] Result<std::monostate> set_mass(BodyId id, double kilograms);
    // A native update-capacity failure closes this domain; it is not rolled
    // back or published as a successful step.
    [[nodiscard]] Result<Tick> step();
    [[nodiscard]] Result<BodyState> state(BodyId id) const;

  private:
    struct Impl;
    explicit World(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> impl_;
};
} // namespace ngen::physics
