#include "core/entities.hpp"
namespace ngen {
Result<Entity> Entities::create() {
    for (std::size_t i = 0; i < slots_.size(); ++i) {
        auto &slot = slots_[i];
        if (!slot.active && slot.generation != 0) {
            slot.active = true;
            slot.position = {};
            return Entity{static_cast<std::uint32_t>(i), slot.generation};
        }
    }
    return Error::capacity;
}
Result<std::monostate> Entities::destroy(Entity entity) {
    if (!position(entity))
        return Error::stale;
    auto &slot = slots_[entity.slot];
    slot.active = false;
    // Retire exhausted slots permanently rather than resurrect old handles.
    const auto generation = next_generation(slot.generation);
    const auto *value = std::get_if<std::uint64_t>(&generation);
    slot.generation = value ? *value : 0;
    return std::monostate{};
}
Millimetres *Entities::position(Entity entity) {
    if (entity.slot >= slots_.size())
        return nullptr;
    auto &slot = slots_[entity.slot];
    return slot.active && slot.generation == entity.generation ? &slot.position : nullptr;
}
const Millimetres *Entities::position(Entity entity) const {
    if (entity.slot >= slots_.size())
        return nullptr;
    const auto &slot = slots_[entity.slot];
    return slot.active && slot.generation == entity.generation ? &slot.position : nullptr;
}
} // namespace ngen
