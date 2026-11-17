#include "simulation/simulation.hpp"
namespace ngen {
Result<Entity> Simulation::create() {
    auto result = entities_.create();
    if (const auto *id = std::get_if<Entity>(&result))
        ids_[entities_count_++] = *id;
    return result;
}
Result<std::monostate> Simulation::submit(Command command) {
    if (command_count_ == command_capacity)
        return Error::capacity;
    const auto expected = next(published_.tick_);
    if (const auto *error = std::get_if<Error>(&expected))
        return *error;
    if (command.tick != std::get<Tick>(expected) || command.sequence <= sequence_)
        return Error::order;
    if (!entities_.position(command.entity))
        return Error::stale;
    commands_[command_count_++] = command;
    sequence_ = command.sequence;
    return std::monostate{};
}
Result<Snapshot> Simulation::step() {
    const auto tick = next(published_.tick_);
    if (const auto *error = std::get_if<Error>(&tick))
        return *error;
    auto staged = entities_;
    for (std::size_t i = 0; i < command_count_; ++i) {
        auto *position = staged.position(commands_[i].entity);
        const auto result = add(*position, commands_[i].displacement);
        if (const auto *error = std::get_if<Error>(&result)) {
            command_count_ = 0; // Reject the whole batch; sequence IDs remain consumed.
            return *error;
        }
        *position = std::get<Millimetres>(result);
    }
    Snapshot candidate;
    candidate.tick_ = std::get<Tick>(tick);
    candidate.size_ = entities_count_;
    struct Extraction {
        const Simulation *simulation;
        const Entities *entities;
        Snapshot *snapshot;
    };
    Extraction extraction{this, &staged, &candidate};
    auto copy = [](void *ptr, const std::atomic<bool> &) noexcept {
        auto &context = *static_cast<Extraction *>(ptr);
        for (std::size_t i = 0; i < context.simulation->entities_count_; ++i) {
            const auto id = context.simulation->ids_[i];
            context.snapshot->poses_[i] = {id, *context.entities->position(id)};
        }
    };
    const auto queued = jobs_.submit(copy, &extraction);
    if (const auto *error = std::get_if<Error>(&queued))
        return *error;
    const auto started = jobs_.start();
    if (const auto *error = std::get_if<Error>(&started)) {
        jobs_.drain();
        return *error;
    }
    jobs_.drain();
    entities_ = staged;
    published_ = candidate;
    command_count_ = 0;
    return published_;
}
} // namespace ngen
