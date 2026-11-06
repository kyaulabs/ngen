#pragma once
#include <functional>

namespace ngen::render {
class Context;
class Image;
} // namespace ngen::render
namespace ngen::probe {
// Synchronous owner-thread check; the optional bounded step runs before each frame attempt.
[[nodiscard]] bool presentation(render::Context &context, const std::function<bool()> &step = {},
                                const render::Image *source = nullptr);
} // namespace ngen::probe
