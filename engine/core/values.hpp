#pragma once
#include <compare>
#include <cstdint>
#include <variant>

namespace ngen {
enum class Error : std::uint8_t { invalid, overflow, capacity, stale, order, closed, busy };
template <class T> using Result = std::variant<T, Error>;
struct Tick {
    std::uint64_t value{};
    auto operator<=>(const Tick &) const = default;
};
struct Millimetres {
    std::int64_t value{};
    auto operator<=>(const Millimetres &) const = default;
};
[[nodiscard]] Result<Millimetres> metres(double value);
[[nodiscard]] Result<Millimetres> add(Millimetres a, Millimetres b);
[[nodiscard]] Result<Tick> next(Tick value);
[[nodiscard]] Result<std::uint64_t> next_generation(std::uint64_t value);
struct Entity {
    std::uint32_t slot{};
    std::uint64_t generation{};
    auto operator<=>(const Entity &) const = default;
};
} // namespace ngen
