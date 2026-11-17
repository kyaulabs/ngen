#pragma once
#include "assetc/import.hpp"

namespace ngen::assetc {
struct TangentSettings {
    std::uint32_t uv_set{};
    std::size_t vertex_allowance{};
    std::size_t fallback_corner_allowance{};
};
[[nodiscard]] bool valid_frame(const Vertex &vertex);
[[nodiscard]] std::variant<TangentStatistics, ImportFailure>
regenerate_tangents(Primitive &primitive, TangentSettings settings);
} // namespace ngen::assetc
