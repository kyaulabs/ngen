#include "assets/package.hpp"
#include "simulation/simulation.hpp"
#include <cstdio>
#include <exception>

int main(int argc, char **argv) try {
    if (argc != 2) {
        std::fputs("usage: ngen_starter <scene.ngas>\n", stderr);
        return 2;
    }
    const auto package = ngen::assets::load_package_file(argv[1]);
    if (const auto *failure = std::get_if<ngen::assets::Failure>(&package)) {
        std::fprintf(stderr, "scene: %s\n", failure->message.c_str());
        return 1;
    }
    const auto &scene = *std::get<ngen::assets::PackagePin>(package);
    if (scene.instances.empty()) {
        std::fputs("scene: expected at least one board marker instance\n", stderr);
        return 1;
    }
    ngen::Simulation simulation;
    const auto entity = simulation.create();
    const auto *id = std::get_if<ngen::Entity>(&entity);
    if (!id || !std::holds_alternative<std::monostate>(simulation.submit({{1}, 1, *id, {42}})))
        return 1;
    const auto step = simulation.step();
    const auto *snapshot = std::get_if<ngen::Snapshot>(&step);
    if (!snapshot || snapshot->tick() != ngen::Tick{1} || snapshot->poses().size() != 1 ||
        snapshot->poses().front() != ngen::Pose{*id, {42}})
        return 1;
    std::printf("Loaded %zu board marker instance(s); piece moved 42 mm.\n",
                scene.instances.size());
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "starter: %s\n", error.what());
    return 1;
}
