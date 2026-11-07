#pragma once
#include "assetc/import.hpp"
#include "assets/package.hpp"

namespace ngen::assetc {
enum class ImageRole : std::uint8_t { base_color, emissive, data, normal, alpha_color };
struct CookSettings {
    std::uint32_t maximum_texture_dimension{2048};
};
struct CookedScene {
    assets::Scene scene;
    std::vector<ImageRole> image_roles;
    std::array<std::uint64_t, 3> lod_triangles{};
    std::size_t texture_bytes{};
};
using CookResult = std::variant<CookedScene, ImportFailure>;
// Offline only. Input is the owning validated result of import_glb; no source
// I/O or native graphics work occurs here. LOD0 retains all source triangles.
[[nodiscard]] CookResult cook_scene(const Scene &source, CookSettings settings = {},
                                    const std::stop_token &stop = {});
} // namespace ngen::assetc
