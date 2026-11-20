#include "travel/system_manifest.hpp"
#include <algorithm>

namespace ngen::travel {
ConnectedSystems streaming_fixture(std::uint32_t seed) {
    constexpr std::int64_t origin = 145'000'000'000;
    constexpr SystemManifest starter{1,
                                     Security::protected_space,
                                     {{{origin, .25}, {}, {}}},
                                     {{{origin, .25}, {0, 400'000}, {}}},
                                     17};
    const SystemManifest frontier{
        2,
        Security::lawless,
        {{{origin + 36, 1.75}, {}, {}}},
        {{{origin + 36, 1.75}, {0, 400'000.0 + static_cast<double>(seed % 4096)}, {}}},
        18};
    return {{starter, frontier}, starter.id};
}

Result<Route> ConnectedSystems::gate(std::uint32_t source, std::uint32_t destination,
                                     bool jump_capable) const {
    const auto find_system = [this](std::uint32_t id) {
        return std::ranges::find(systems, id, &SystemManifest::id);
    };
    const auto from = find_system(source);
    const auto to = find_system(destination);
    if (source == destination || from == systems.end() || to == systems.end())
        return Error::invalid;
    // The fixture's gate is bidirectional and both jurisdictions allow passage.
    return Route{from->berth, to->berth, 8, to->collision_revision, jump_capable, true, true};
}
} // namespace ngen::travel
