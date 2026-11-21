#pragma once
#include "render/scene_draw.hpp"
#include <filesystem>
namespace ngen::probe {
// With an empty active pin, verify one offscreen draw without consuming version.
// Otherwise consume version to verify owning frame publication.
bool draw_scene(render::Context &context, render::ScenePin &version, const render::ScenePin &active,
                const std::filesystem::path &capture = {}, bool particle_fixture = false);
} // namespace ngen::probe
