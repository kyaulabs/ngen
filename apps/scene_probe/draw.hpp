#pragma once
#include "render/scene_draw.hpp"
#include <filesystem>
namespace ngen::probe {
// Consumes the caller's second version to verify owning frame publication.
bool draw_scene(render::Context &context, render::ScenePin &version, const render::ScenePin &active,
                const std::filesystem::path &capture = {});
} // namespace ngen::probe
