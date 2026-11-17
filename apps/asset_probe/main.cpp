#include "assets/package.hpp"
#include <algorithm>
#include <exception>
#include <iostream>

int main(int argc, char **argv) try {
    if (argc != 2) {
        std::cerr << "usage: asset_package_probe <scene.ngas>\n";
        return 2;
    }
    const auto result = ngen::assets::load_package_file(std::filesystem::path{argv[1]});
    if (const auto *failure = std::get_if<ngen::assets::Failure>(&result)) {
        std::cerr << "asset_package_probe: " << failure->message << '\n';
        return 1;
    }
    const auto &scene = *std::get<ngen::assets::PackagePin>(result);
    std::array<std::uint64_t, 3> triangles{};
    for (const auto &primitive : scene.primitives)
        for (std::size_t lod = 0; lod < triangles.size(); ++lod)
            triangles[lod] +=
                primitive.lods[std::min(lod, primitive.lods.size() - 1)].index_count / 3;
    std::uint64_t texture_bytes = 0;
    for (const auto &image : scene.images)
        for (const auto &mip : image.mips)
            texture_bytes += mip.pixels.size();
    std::cout << "{\"package_valid\":1,\"vertices\":" << scene.vertices.size()
              << ",\"indices\":" << scene.indices.size()
              << ",\"primitives\":" << scene.primitives.size()
              << ",\"instances\":" << scene.instances.size()
              << ",\"materials\":" << scene.materials.size()
              << ",\"images\":" << scene.images.size() << ",\"textures\":" << scene.textures.size()
              << ",\"lights\":" << scene.lights.size()
              << ",\"light_instances\":" << scene.light_instances.size()
              << ",\"texture_bytes\":" << texture_bytes << ",\"lod_triangles\":[" << triangles[0]
              << ',' << triangles[1] << ',' << triangles[2] << "],\"checksum\":" << scene.checksum
              << "}\n";
    return 0;
} catch (const std::exception &error) {
    std::cerr << "asset_package_probe: " << error.what() << '\n';
    return 1;
}
