#include "assetc/import.hpp"
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {
void vector3(const std::array<float, 3> &value) {
    std::cout << '[' << value[0] << ',' << value[1] << ',' << value[2] << ']';
}
} // namespace
int main(int argc, char **argv) {
    if ((argc != 3 && argc != 4) || std::string_view{argv[1]} != "inspect" ||
        (argc == 4 && std::string_view{argv[2]} != "--rebuild-tangents")) {
        std::cerr << "usage: assetc inspect [--rebuild-tangents] <embedded static scene.glb>\n";
        return 2;
    }
    try {
        const std::filesystem::path path{argv[argc - 1]};
        if (!std::filesystem::is_regular_file(path))
            throw std::runtime_error("source is not a readable regular file");
        const auto size = std::filesystem::file_size(path);
        if (size > ngen::assetc::source_byte_limit)
            throw std::runtime_error("source byte limit exceeded");
        std::ifstream input(path, std::ios::binary);
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        if (!bytes.empty())
            input.read(reinterpret_cast<char *>(bytes.data()),
                       static_cast<std::streamsize>(bytes.size()));
        if (!input || input.peek() != std::char_traits<char>::eof())
            throw std::runtime_error("source read failed or file size changed");
        auto imported = ngen::assetc::import_glb(bytes, {.rebuild_tangents = argc == 4});
        if (const auto *failure = std::get_if<ngen::assetc::ImportFailure>(&imported))
            throw std::runtime_error(failure->message);
        const auto &scene = std::get<ngen::assetc::Scene>(imported);
        std::uint64_t vertices = 0;
        std::uint64_t indices = 0;
        std::uint64_t instance_triangles = 0;
        std::uint64_t decoded_image_bytes = 0;
        for (const auto &primitive : scene.primitives) {
            vertices += primitive.vertices.size();
            indices += primitive.indices.size();
        }
        for (const auto &instance : scene.instances)
            instance_triangles += scene.primitives[instance.primitive].indices.size() / 3;
        for (const auto &image : scene.images)
            decoded_image_bytes += image.pixels.size();
        std::cout << std::setprecision(9) << "{\"meshes\":" << scene.meshes.size()
                  << ",\"primitives\":" << scene.primitives.size() << ",\"vertices\":" << vertices
                  << ",\"source_vertices\":" << scene.source_vertices << ",\"indices\":" << indices
                  << ",\"triangles\":" << indices / 3 << ",\"instances\":" << scene.instances.size()
                  << ",\"instance_triangles\":" << instance_triangles
                  << ",\"nodes\":" << scene.source_nodes
                  << ",\"materials\":" << scene.materials.size()
                  << ",\"images\":" << scene.images.size()
                  << ",\"decoded_image_bytes\":" << decoded_image_bytes
                  << ",\"textures\":" << scene.textures.size()
                  << ",\"samplers\":" << scene.samplers.size()
                  << ",\"lights\":" << scene.lights.size()
                  << ",\"light_instances\":" << scene.light_instances.size()
                  << ",\"invalid_source_tangents\":" << scene.tangents.invalid_source_vertices
                  << ",\"missing_source_tangents\":" << scene.tangents.missing_source_vertices
                  << ",\"rebuilt_primitives\":" << scene.tangents.rebuilt_primitives
                  << ",\"rebuilt_corners\":" << scene.tangents.rebuilt_corners
                  << ",\"tangent_derivative_fallback_corners\":"
                  << scene.tangents.derivative_fallback_corners
                  << ",\"degenerate_uv_triangles\":" << scene.tangents.degenerate_uv_triangles
                  << ",\"degenerate_uv_projection_corners\":"
                  << scene.tangents.degenerate_projection_corners << ",\"bounds\":{\"minimum\":";
        vector3(scene.bounds.minimum);
        std::cout << ",\"maximum\":";
        vector3(scene.bounds.maximum);
        std::cout << "}}\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "assetc: " << error.what() << '\n';
        return 1;
    }
}
