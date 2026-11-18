#include "physics/world.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <exception>

using ngen::Error;
using ngen::physics::BodyId;
using ngen::physics::World;

int main() try {
    for (const float size : {10.0F, 100.0F, 1000.0F}) {
        const auto half = size / 2;
        for (const double origin : {-4096.0, 4096.0}) {
            auto created = World::create();
            const auto &world = std::get<std::unique_ptr<World>>(created);
            const auto wall = world->add_box({.half_extent_metres = {half, half, half},
                                              .position_metres = {origin, 0, 0},
                                              .dynamic = false});
            if (size == 1000) {
                const auto *failure = std::get_if<Error>(&wall);
                if (!failure || *failure != Error::invalid)
                    return 1;
                std::printf("hull %.0f m; center %.0f m: single box rejected\n",
                            static_cast<double>(size), origin);
                continue;
            }
            if (!std::holds_alternative<BodyId>(wall))
                return 2;
            const auto boundary = origin - static_cast<double>(half) - 5;
            const auto mover = world->add_box(
                {.half_extent_metres = {5, 5, 5}, .position_metres = {boundary - 2, 0, 0}});
            const auto id = std::get<BodyId>(mover);
            double closest = -2;
            float speed = 0;
            for (unsigned tick = 0; tick < 120; ++tick) {
                if (!std::holds_alternative<std::monostate>(world->apply(id, {6000, 0, 0})) ||
                    !std::holds_alternative<ngen::Tick>(world->step()))
                    return 3;
                const auto state = std::get<ngen::physics::BodyState>(world->state(id));
                const auto relative = state.position_metres[0] - boundary;
                speed = state.velocity_mps[0];
                if (!std::isfinite(relative) || !std::isfinite(speed))
                    return 4;
                closest = std::max(closest, relative);
            }
            std::printf("hull %.0f m; center %.0f m: peak center advance past touch %.9g m; "
                        "final x speed %.9g m/s\n",
                        static_cast<double>(size), origin, closest, static_cast<double>(speed));
            // Broad discrimination only: distinguish a stopping contact from a pass-through.
            // These are not proposed acceptance tolerances for the later rebase experiment.
            if (closest < -0.1 || closest > 0.1 || std::abs(speed) > 0.1F)
                return 5;
        }
    }
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "contact probe: %s\n", error.what());
    return 6;
} catch (...) {
    std::fputs("contact probe: unknown failure\n", stderr);
    return 6;
}
