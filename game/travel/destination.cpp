#include "travel/destination.hpp"
#include <algorithm>

namespace ngen::travel {
Result<std::monostate> DestinationCollision::publish(const CollisionSite &site) {
    if (site.origin != origin_ || site.revision != revision_)
        return Error::stale;
    if (!site.revision)
        return Error::invalid;
    if (site.obstacles.size() > obstacles_.size())
        return Error::capacity;
    if (std::ranges::any_of(site.obstacles, &physics::Box::dynamic))
        return Error::invalid;
    std::ranges::copy(site.obstacles, obstacles_.begin());
    count_ = site.obstacles.size();
    return std::monostate{};
}

Result<Snapshot> DestinationCollision::arrive(Journey &journey, const Command &command,
                                              std::uint64_t trip) const {
    if (!count_)
        return journey.arrive(command, trip, nullptr);
    const CollisionSite site{origin_, revision_, std::span{obstacles_}.first(*count_)};
    return journey.arrive(command, trip, &site);
}
} // namespace ngen::travel
