#include "render/capabilities.hpp"
namespace ngen::render {
std::optional<Missing> check(const Capabilities &c) {
    if (c.api_major < 1 || (c.api_major == 1 && c.api_minor < 3))
        return Missing::api;
    if (!c.graphics)
        return Missing::graphics;
    if (!c.presentation)
        return Missing::presentation;
    if (!c.dynamic_rendering)
        return Missing::dynamic_rendering;
    if (!c.synchronization2)
        return Missing::synchronization2;
    if (!c.timeline_semaphore)
        return Missing::timeline_semaphore;
    if (!c.color_attachment)
        return Missing::color_attachment;
    if (!c.depth_attachment)
        return Missing::depth_attachment;
    return std::nullopt;
}
} // namespace ngen::render
