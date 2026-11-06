#pragma once
#include <functional>

namespace ngen::render {
class Context;
}
namespace ngen::probe {
// Synchronous owner-thread check; the optional bounded step runs before each frame attempt.
[[nodiscard]] bool presentation(render::Context &context, const std::function<bool()> &step = {});
} // namespace ngen::probe
