#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>

namespace ngen::gpu_memory {
inline constexpr std::size_t instance_limit = 64;
inline constexpr std::size_t name_limit = 128;
struct Counter {
    std::string_view name;
    std::int64_t value{};
};
struct Instance {
    std::array<char, name_limit + 1> name{};
    std::uint64_t dedicated{}, shared{}, committed{};
};
struct Snapshot {
    std::array<Instance, instance_limit> instances{};
    std::size_t count{};
};
enum class Error : std::uint8_t { capacity, instance, duplicate, value, incomplete };
// Input names borrow their collector buffers only for this call; results own all data.
[[nodiscard]] std::variant<Snapshot, Error> join(std::uint32_t process_id,
                                                 std::span<const Counter> dedicated,
                                                 std::span<const Counter> shared,
                                                 std::span<const Counter> committed) noexcept;
} // namespace ngen::gpu_memory
