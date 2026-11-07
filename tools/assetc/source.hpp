#pragma once
#include <cstddef>
#include <filesystem>
#include <vector>
namespace ngen::assetc {
// CLI boundary; throws a diagnostic on nonregular, oversized or changing input.
[[nodiscard]] std::vector<std::byte> read_source(const std::filesystem::path &path);
} // namespace ngen::assetc
