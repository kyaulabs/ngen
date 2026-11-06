#pragma once
#include "assetc/import.hpp"

namespace ngen::assetc {
// Private tooling decoder. Limits are checked before output storage allocation.
[[nodiscard]] std::variant<Image, ImportFailure> decode_png(std::span<const std::byte> bytes,
                                                            std::size_t byte_allowance);
} // namespace ngen::assetc
