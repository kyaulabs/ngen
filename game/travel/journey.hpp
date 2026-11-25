#pragma once
#include "flight/flight.hpp"
#include "travel/trajectory.hpp"

namespace ngen::travel {
// Owner-supplied route and policy, not fields in a pilot command. A revision
// names the destination's prepared collision profile, including an empty site.
struct Route {
    coordinates::Position source, destination;
    std::uint32_t duration_ticks{};
    std::uint64_t collision_revision{};
    bool jump_capable{}, route_allowed{}, security_allowed{};
};
struct Command {
    Entity vessel;
    Tick expected;
    std::uint64_t sequence{};
};
struct CollisionSite {
    coordinates::Position origin;
    std::uint64_t revision{};
    std::span<const physics::Box> obstacles;
};
enum class Phase : std::uint8_t { local, entry, transit, exit, arrived };
struct Snapshot {
    Phase phase{Phase::local};
    Tick tick;
    Entity vessel;
    std::uint64_t trip{};
    // Route sample or berth anchor, not the position of a local contact body.
    coordinates::Position route_position;
    std::optional<flight::Snapshot> local;
};

// Single owner thread, one vessel and one authored, one-way route. Entry is
// cancellable at the current tick barrier; the next step commits the trip.
// No local contact body survives committed transit. Arrival prepares a fresh
// contact domain before publication and does not reset the session tick. Callers
// route commands to their owning Journey; vessel identity is session-local.
class Journey {
  public:
    [[nodiscard]] static Result<std::unique_ptr<Journey>>
    create(const Route &route, const flight::ShipDefinition &ship,
           std::span<const physics::Box> source_obstacles = {}, Tick initial_tick = {});
    Journey(const Journey &) = delete;
    Journey &operator=(const Journey &) = delete;
    [[nodiscard]] Snapshot snapshot() const;
    [[nodiscard]] Result<std::monostate> submit(const flight::Command &command);
    [[nodiscard]] Result<Snapshot> begin(const Command &command);
    [[nodiscard]] Result<Snapshot> cancel(const Command &command, std::uint64_t trip);
    [[nodiscard]] Result<Snapshot> step();
    // Null means not ready. Borrow a same-owner prepared site only for this call.
    [[nodiscard]] Result<Snapshot> arrive(const Command &command, std::uint64_t trip,
                                          const CollisionSite *site);

  private:
    Journey(Route route, flight::ShipDefinition ship, Trajectory path,
            std::unique_ptr<flight::Flight> local);
    [[nodiscard]] Result<std::monostate> check(const Command &command) const;
    const std::thread::id owner_{std::this_thread::get_id()};
    Route route_;
    flight::ShipDefinition ship_;
    Trajectory path_;
    std::unique_ptr<flight::Flight> flight_;
    Snapshot state_;
    std::uint64_t sequence_{};
    std::uint32_t elapsed_{};
    bool pending_{};
};
} // namespace ngen::travel
