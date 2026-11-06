#pragma once
#include "core/entities.hpp"
#include "jobs/jobs.hpp"
#include <array>
#include <span>
namespace ngen {
struct Command {
    Tick tick;
    std::uint64_t sequence;
    Entity entity;
    Millimetres displacement;
};
struct Pose {
    Entity entity;
    Millimetres position;
    auto operator<=>(const Pose &) const = default;
};
class Snapshot {
  public:
    [[nodiscard]] Tick tick() const { return tick_; }
    [[nodiscard]] std::span<const Pose> poses() const { return {poses_.data(), size_}; }

  private:
    friend class Simulation;
    Tick tick_{};
    std::array<Pose, Entities::capacity> poses_{};
    std::size_t size_{};
};
class Simulation {
  public:
    static constexpr std::size_t command_capacity = 64;
    static constexpr unsigned ticks_per_second = 60;
    explicit Simulation(unsigned workers = 0) : jobs_(workers) {}
    [[nodiscard]] Result<Entity> create();
    [[nodiscard]] Result<std::monostate> submit(Command command);
    [[nodiscard]] Result<Snapshot> step();
    [[nodiscard]] Snapshot snapshot() const { return published_; }

  private:
    Jobs jobs_;
    Entities entities_;
    std::array<Entity, Entities::capacity> ids_{};
    std::size_t entities_count_{};
    std::array<Command, command_capacity> commands_{};
    std::size_t command_count_{};
    std::uint64_t sequence_{};
    Snapshot published_{};
};
} // namespace ngen
