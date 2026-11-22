#pragma once
#include "travel/journey.hpp"

namespace ngen::travel {
// One owner-thread staging slot for a route's collision data. The producer
// supplies checked data; this slot neither loads assets nor starts workers.
class DestinationCollision {
  public:
    explicit DestinationCollision(const Route &route)
        : origin_(route.destination), revision_(route.collision_revision) {}
    [[nodiscard]] Result<std::monostate> publish(const CollisionSite &site);
    [[nodiscard]] Result<Snapshot> arrive(Journey &journey, const Command &command,
                                          std::uint64_t trip) const;

  private:
    coordinates::Position origin_;
    std::uint64_t revision_;
    std::array<physics::Box, physics::World::body_capacity - 1> obstacles_{};
    std::optional<std::size_t> count_;
};
} // namespace ngen::travel
