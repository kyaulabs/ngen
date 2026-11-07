#include "assetc/stage.hpp"
#include "assetc/cook.hpp"
#include "assetc/source.hpp"
#include <bit>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace ngen::assetc {
namespace {
struct Bytes {
    std::vector<std::byte> data;
    void u32(std::uint32_t value) {
        for (unsigned i = 0; i < 4; ++i)
            data.push_back(static_cast<std::byte>((value >> (i * 8U)) & 255U));
    }
    void f32(float value) { u32(std::bit_cast<std::uint32_t>(value)); }
    template <std::size_t N> void floats(const std::array<float, N> &values) {
        for (float value : values)
            f32(value);
    }
};
void write_file(const std::filesystem::path &path, std::span<const std::byte> data) {
    if (data.size() > assets::package_byte_limit)
        throw std::runtime_error("staging file byte limit exceeded");
    std::ofstream output(path, std::ios::binary);
    if (!data.empty())
        output.write(reinterpret_cast<const char *>(data.data()),
                     static_cast<std::streamsize>(data.size()));
    output.close();
    if (!output)
        throw std::runtime_error("cannot write scene staging file");
}
struct Section {
    std::uint32_t kind, stride, count;
};
std::vector<Section> stage_sections(const assets::Scene &scene,
                                    const std::filesystem::path &directory) {
    std::vector<Section> catalog;
    Bytes out;
    const auto section = [&](std::uint32_t kind, std::uint32_t stride) {
        if (out.data.size() % stride != 0)
            throw std::runtime_error("invalid native section encoding");
        write_file(directory / ("section-" + std::to_string(kind) + ".bin"), out.data);
        catalog.push_back({kind, stride, static_cast<std::uint32_t>(out.data.size() / stride)});
        out.data.clear();
    };
    for (const auto &vertex : scene.vertices) {
        out.floats(vertex.position);
        out.floats(vertex.normal);
        out.floats(vertex.uv);
        out.floats(vertex.uv1);
        out.floats(vertex.tangent);
    }
    section(1, 56);
    for (auto index : scene.indices)
        out.u32(index);
    section(2, 4);
    for (const auto &primitive : scene.primitives) {
        out.u32(primitive.first_vertex);
        out.u32(primitive.vertex_count);
        out.u32(primitive.material);
        out.u32(static_cast<std::uint32_t>(primitive.lods.size()));
        out.floats(primitive.bounds.minimum);
        out.floats(primitive.bounds.maximum);
        if (primitive.lods.size() > 3)
            throw std::runtime_error("too many cooked LODs");
        for (std::size_t i = 0; i < 3; ++i) {
            const auto lod = i < primitive.lods.size() ? primitive.lods[i] : assets::Lod{};
            out.u32(lod.first_index);
            out.u32(lod.index_count);
            out.f32(lod.error);
        }
        out.u32(0);
    }
    section(3, 80);
    Bytes names;
    for (const auto &material : scene.materials) {
        out.floats(material.base_color);
        out.floats(material.emissive);
        out.f32(material.emissive_strength);
        out.f32(material.metallic);
        out.f32(material.roughness);
        out.f32(material.normal_scale);
        out.f32(material.occlusion_strength);
        out.f32(material.alpha_cutoff);
        out.u32(material.alpha);
        out.u32(material.double_sided ? 1U : 0U);
        for (const auto &map : material.maps) {
            out.u32(map.texture);
            out.u32(map.uv_set);
        }
        out.u32(static_cast<std::uint32_t>(names.data.size()));
        out.u32(static_cast<std::uint32_t>(material.name.size()));
        for (char c : material.name)
            names.data.push_back(static_cast<std::byte>(static_cast<unsigned char>(c)));
        for (unsigned i = 0; i < 5; ++i)
            out.u32(0);
    }
    section(4, 128);
    for (const auto &texture : scene.textures) {
        out.u32(texture.image);
        out.u32(texture.sampler);
    }
    section(5, 8);
    for (const auto &sampler : scene.samplers) {
        out.u32(sampler.min);
        out.u32(sampler.mag);
        out.u32(sampler.s);
        out.u32(sampler.t);
    }
    section(6, 16);
    for (const auto &instance : scene.instances) {
        out.u32(instance.primitive);
        out.u32(instance.source_node);
        out.floats(instance.transform);
    }
    section(9, 72);
    for (const auto &light : scene.lights) {
        out.u32(light.type);
        out.floats(light.color);
        out.f32(light.intensity);
        out.f32(light.range);
        out.f32(light.inner_angle);
        out.f32(light.outer_angle);
    }
    section(10, 32);
    for (const auto &instance : scene.light_instances) {
        out.u32(instance.light);
        out.u32(instance.source_node);
        out.floats(instance.transform);
    }
    section(11, 72);
    out = std::move(names);
    section(12, 1);
    return catalog;
}
std::string stage_images(const CookedScene &cooked, const std::filesystem::path &directory) {
    std::ostringstream json;
    json << '[';
    for (std::size_t image = 0; image < cooked.scene.images.size(); ++image) {
        if (image != 0)
            json << ',';
        const auto &value = cooked.scene.images[image];
        if (value.mips.empty())
            throw std::runtime_error("cooked image has no mip chain");
        json << "{\"width\":" << value.mips[0].width << ",\"height\":" << value.mips[0].height
             << ",\"format\":" << static_cast<unsigned>(value.format)
             << ",\"role\":" << static_cast<unsigned>(cooked.image_roles[image]) << ",\"files\":[";
        for (std::size_t mip = 0; mip < value.mips.size(); ++mip) {
            if (mip != 0)
                json << ',';
            const auto filename =
                "image-" + std::to_string(image) + "-mip-" + std::to_string(mip) + ".rgba";
            write_file(directory / filename, value.mips[mip].pixels);
            json << '"' << filename << '"';
        }
        json << "]}";
    }
    json << ']';
    return json.str();
}
} // namespace
int stage_main(int argc, char **argv) {
    if (argc != 6 || std::string_view{argv[2]} != "--preset" ||
        std::string_view{argv[3]} != "laptop") {
        std::cerr << "usage: assetc stage --preset laptop <source.glb> <new-staging-directory>\n";
        return 2;
    }
    const std::filesystem::path directory{argv[5]};
    bool created = false;
    try {
        auto bytes = read_source(std::filesystem::path{argv[4]});
        auto imported = import_glb(bytes, {.rebuild_tangents = true});
        if (const auto *failure = std::get_if<ImportFailure>(&imported))
            throw std::runtime_error(failure->message);
        bytes.clear();
        bytes.shrink_to_fit();
        const auto &source = std::get<Scene>(imported);
        auto result = cook_scene(source);
        if (const auto *failure = std::get_if<ImportFailure>(&result))
            throw std::runtime_error(failure->message);
        const auto &cooked = std::get<CookedScene>(result);
        if (!std::filesystem::create_directory(directory))
            throw std::runtime_error("staging directory must not already exist");
        created = true;
        const auto sections = stage_sections(cooked.scene, directory);
        const auto images = stage_images(cooked, directory);
        std::ostringstream report;
        report << "{\"schema\":1,\"preset\":\"laptop\",\"source_vertices\":"
               << source.source_vertices << ",\"source_nodes\":" << source.source_nodes
               << ",\"source_meshes\":" << source.meshes.size()
               << ",\"maximum_texture_dimension\":2048,\"texture_bytes\":" << cooked.texture_bytes
               << ",\"lod_triangles\":[";
        for (std::size_t i = 0; i < 3; ++i) {
            if (i != 0)
                report << ',';
            report << cooked.lod_triangles[i];
        }
        report << "],\"invalid_source_tangents\":" << source.tangents.invalid_source_vertices
               << ",\"rebuilt_corners\":" << source.tangents.rebuilt_corners
               << ",\"images\":" << images << ",\"sections\":[";
        for (std::size_t i = 0; i < sections.size(); ++i) {
            if (i != 0)
                report << ',';
            const auto &value = sections[i];
            report << "{\"kind\":" << value.kind << ",\"stride\":" << value.stride
                   << ",\"count\":" << value.count << ",\"file\":\"section-" << value.kind
                   << ".bin\"}";
        }
        report << "]}\n";
        const auto text = report.str();
        write_file(directory / "stage.json", std::as_bytes(std::span{text.data(), text.size()}));
        std::cout << text;
        return 0;
    } catch (const std::exception &failure) {
        if (created) {
            std::error_code ignored;
            std::filesystem::remove_all(directory, ignored);
        }
        std::cerr << "assetc stage: " << failure.what() << '\n';
        return 1;
    }
}
} // namespace ngen::assetc
