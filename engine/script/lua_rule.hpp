#pragma once
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <variant>

namespace ngen::script {
struct Facts {
    std::uint8_t stage{};
    std::uint8_t event{};
};
struct Limits {
    std::size_t heap_bytes{std::size_t{64} * 1024};
    int instructions{4096};
};
enum class Failure : std::uint8_t { input, syntax, runtime, heap, instructions, result };
using Decision = std::variant<std::uint8_t, Failure>;

// Evaluate first-party text in a fresh VM with no standard libraries or host
// callbacks. The scalar result is a proposal; the caller owns authorization.
[[nodiscard]] Decision evaluate(std::string_view source, Facts facts, Limits limits = {});
} // namespace ngen::script
