#include "simulation/simulation.hpp"

int main() {
    ngen::Simulation simulation{2};
    const auto entity = simulation.create();
    const auto *id = std::get_if<ngen::Entity>(&entity);
    if (!id || !std::holds_alternative<std::monostate>(simulation.submit({{1}, 1, *id, {42}})))
        return 1;
    const auto step = simulation.step();
    const auto *snapshot = std::get_if<ngen::Snapshot>(&step);
    if (!snapshot || snapshot->tick() != ngen::Tick{1} || snapshot->poses().size() != 1 ||
        snapshot->poses().front() != ngen::Pose{*id, {42}})
        return 2;
    return 0;
}
