#pragma once
#include "travel/journey.hpp"

namespace ngen::travel {
enum class Security : std::uint8_t { protected_space, lawless };
struct SystemManifest {
    std::uint32_t id{};
    Security security{};
    coordinates::Position berth;
    coordinates::Position celestial;
    std::uint64_t collision_revision{};
};
struct ConnectedSystems {
    const std::array<SystemManifest, 2> systems;
    const std::uint32_t starter_system;

    [[nodiscard]] Result<Route> gate(std::uint32_t source, std::uint32_t destination,
                                     bool jump_capable) const;
};

// Bounded fixture, not a production catalog or a policing implementation.
[[nodiscard]] ConnectedSystems streaming_fixture(std::uint32_t seed);
} // namespace ngen::travel
