#pragma once
#include <cstdint>
#include <optional>

namespace ngen::render {
struct Capabilities {
    std::uint32_t api_major{};
    std::uint32_t api_minor{};
    bool graphics{};
    bool presentation{};
    bool dynamic_rendering{};
    bool synchronization2{};
    bool timeline_semaphore{};
    bool color_attachment{};
    bool depth_attachment{};
};
enum class Missing : std::uint8_t {
    api,
    graphics,
    presentation,
    dynamic_rendering,
    synchronization2,
    timeline_semaphore,
    color_attachment,
    depth_attachment
};
[[nodiscard]] std::optional<Missing> check(const Capabilities &capabilities);
} // namespace ngen::render
