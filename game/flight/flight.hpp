#pragma once
#include "physics/world.hpp"
#include <array>
#include <memory>
#include <span>
#include <thread>
#include <variant>

namespace ngen::flight {
enum class Mode : std::uint8_t { tactical, direct };
struct CameraMount {
    std::array<double, 3> position_metres{};
    std::array<float, 4> rotation_xyzw{0, 0, 0, 1};
    float vertical_fov_degrees{70};
};
struct ShipDefinition {
    physics::Box hull{{2, 1, 1}, {}, 1000};
    // Independent body-local actuator limits. Forward is local -Z; up is +Y.
    std::array<float, 3> maximum_force_newtons{6000, 6000, 6000};
    std::array<float, 3> maximum_torque_newton_metres{2000, 2000, 2000};
    float maximum_speed_mps{100}, maximum_angular_speed_rps{.5F};
    // Authored per ship; presentation must not substitute a universal offset.
    CameraMount flight_camera;
};
struct Pilot {
    std::array<float, 3> thrust{}, rotation{};
    bool brake{};
};
struct SetMode {
    Mode mode;
};
struct Destination {
    std::array<double, 3> position_metres;
};
struct Orbit {
    std::array<double, 3> centre_metres;
    float radius_metres{100}, speed_mps{10};
    bool clockwise{};
};
struct SetMass {
    double kilograms;
};
enum class Goal : std::uint8_t { coast, destination, orbit };
struct Navigation {
    std::array<double, 3> target_metres{};
    float radius_metres{}, speed_mps{};
    Goal goal{Goal::coast};
    bool clockwise{};
};
using Intent = std::variant<Pilot, SetMode, Destination, Orbit, SetMass>;
struct Command {
    Tick tick;
    std::uint64_t sequence{};
    Entity entity;
    Intent intent;
};
struct Snapshot {
    Entity entity;
    Tick tick;
    Mode mode{Mode::tactical};
    physics::BodyState body;
    CameraMount flight_camera;
    Navigation navigation;
    // World-space actuation applied over this tick; body is the end-of-tick state.
    std::array<float, 3> force_newtons{}, torque_newton_metres{};
};
// Owner-thread-only. Copies of snapshots may cross threads; the Flight may not.
// Tick-indexed intent only: no wall clock, window, renderer, audio or services.
class Flight {
  public:
    static constexpr std::size_t command_capacity = 64;
    // Copy at most127 static contact proxies into the same bounded domain.
    // Failed preparation publishes no Flight; reset preserves the copied preset.
    [[nodiscard]] static Result<std::unique_ptr<Flight>>
    create(const ShipDefinition &definition, std::span<const physics::Box> static_obstacles = {});
    ~Flight();
    Flight(const Flight &) = delete;
    Flight &operator=(const Flight &) = delete;
    // Copy next-tick intents. Invalid admission consumes no sequence or capacity.
    [[nodiscard]] Result<std::monostate> submit(const Command &command);
    // Infeasible staged orbit/mass combinations drop the whole batch without
    // advancing or changing native state. Native failures close until reset.
    [[nodiscard]] Result<Snapshot> step();
    // Replace the physical scene only after successful preparation, advance the
    // entity generation and discard queued intents and prior pilot input.
    [[nodiscard]] Result<Snapshot> reset();
    [[nodiscard]] Snapshot snapshot() const;

  private:
    struct Impl;
    explicit Flight(std::unique_ptr<Impl> impl);
    const std::thread::id owner_{std::this_thread::get_id()};
    std::unique_ptr<Impl> impl_;
};
} // namespace ngen::flight
