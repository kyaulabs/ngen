// SPDX-License-Identifier: AGPL-3.0-or-later
// The docking beacon uses NGEN's original starter triangle (see LICENSE).
#include "assets/package.hpp"
#include "simulation/simulation.hpp"
#include <cinttypes>
#include <cstdio>
#include <exception>
#include <stdexcept>

int main(int argc, char **argv) try {
    if (argc != 2) {
        std::fputs("usage: ngen_starter <scene.ngas>\n", stderr);
        return 2;
    }
    const auto package = ngen::assets::load_package_file(argv[1]);
    const auto *beacon = std::get_if<ngen::assets::PackagePin>(&package);
    if (!beacon || (*beacon)->instances.size() != 1) {
        std::fputs("docking: expected a cooked scene with one beacon\n", stderr);
        return 1;
    }

    ngen::Simulation simulation;
    const auto freighter = simulation.create();
    const auto *id = std::get_if<ngen::Entity>(&freighter);
    if (!id)
        return 1;

    const auto advance = [&](ngen::Tick tick, ngen::Millimetres movement) {
        const ngen::Command command{tick, tick.value, *id, movement};
        if (!std::holds_alternative<std::monostate>(simulation.submit(command)))
            throw std::runtime_error("approach command rejected");
        auto step = simulation.step();
        const auto *snapshot = std::get_if<ngen::Snapshot>(&step);
        if (!snapshot || snapshot->tick() != tick || snapshot->poses().size() != 1 ||
            snapshot->poses().front().entity != *id)
            throw std::runtime_error("missing freighter pose");
        return snapshot->poses().front().position;
    };

    const auto approach = advance({1}, {125});
    std::printf("Tick 1: approach at %" PRId64 " mm.\n", approach.value);
    const auto docking = advance({2}, {75});
    constexpr ngen::Millimetres berth{200};
    if (docking != berth) {
        std::fputs("docking: freighter missed the berth\n", stderr);
        return 1;
    }
    std::printf("Tick 2: docked at %" PRId64 " mm.\n", docking.value);
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "docking: %s\n", error.what());
    return 1;
}
