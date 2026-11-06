#pragma once
#include "core/values.hpp"
#include <array>
#include <cstddef>

namespace ngen {
// Fixed fixture storage, not an ECS. Borrowed pointers expire on mutation.
class Entities {
  public:
    static constexpr std::size_t capacity = 16;
    [[nodiscard]] Result<Entity> create();
    [[nodiscard]] Result<std::monostate> destroy(Entity entity);
    [[nodiscard]] Millimetres *position(Entity entity);
    [[nodiscard]] const Millimetres *position(Entity entity) const;

  private:
    struct Slot {
        Millimetres position{};
        std::uint64_t generation{1};
        bool active{};
    };
    std::array<Slot, capacity> slots_{};
};
} // namespace ngen
