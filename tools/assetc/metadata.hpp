#pragma once
#include "assetc/import.hpp"
#include <optional>

namespace ngen::assetc {
[[nodiscard]] std::optional<ImportFailure> check_metadata(std::span<const std::byte> json);
} // namespace ngen::assetc
