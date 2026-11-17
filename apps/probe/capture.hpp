#pragma once
#include "render/context.hpp"
#include <filesystem>

namespace ngen::probe {
// Cold evidence output, never a measured frame-loop operation. No overwrite.
[[nodiscard]] std::variant<std::monostate, render::Failure>
write_exclusive(const std::filesystem::path &path, std::span<const std::byte> bytes);
[[nodiscard]] std::variant<std::monostate, render::Failure>
capture(render::Context &context, const render::Image &hdr, VkExtent2D extent,
        const std::filesystem::path &path, float exposure = 1, bool fxaa = true,
        const render::Overlay *overlay = nullptr, float bloom = 0);
} // namespace ngen::probe
