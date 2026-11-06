#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <stop_token>
#include <string>
#include <variant>
#include <vector>

namespace ngen::assets {
inline constexpr std::size_t package_byte_limit = 256ULL * 1024 * 1024;
inline constexpr std::size_t vertex_limit = 4ULL * 1024 * 1024;
inline constexpr std::size_t index_limit = 18ULL * 1024 * 1024;
inline constexpr std::size_t primitive_limit = 4096;
struct Vertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
    std::array<float, 2> uv{};
    std::array<float, 2> uv1{};
    std::array<float, 4> tangent{};
};
struct Bounds {
    std::array<float, 3> minimum{}, maximum{};
};
struct Lod {
    std::uint32_t first_index{}, index_count{};
    float error{};
};
struct Primitive {
    std::uint32_t first_vertex{}, vertex_count{}, material{UINT32_MAX};
    Bounds bounds;
    std::vector<Lod> lods;
};
struct TextureReference {
    std::uint32_t texture{UINT32_MAX}, uv_set{};
};
struct Material {
    std::string name;
    std::array<float, 4> base_color{1, 1, 1, 1};
    std::array<float, 3> emissive{};
    float emissive_strength{1}, metallic{1}, roughness{1}, normal_scale{1}, occlusion_strength{1},
        alpha_cutoff{0.5F};
    std::uint32_t alpha{};
    bool double_sided{};
    std::array<TextureReference, 5>
        maps; // Base color, metallic/roughness, normal, occlusion, emissive.
};
struct Texture {
    std::uint32_t image{}, sampler{UINT32_MAX};
};
struct Sampler {
    std::uint32_t min{}, mag{}, s{10497}, t{10497};
};
enum class ImageFormat : std::uint8_t { rgba8_unorm = 1, rgba8_srgb = 2 };
struct Mip {
    std::uint32_t width{}, height{};
    std::vector<std::byte> pixels;
};
struct Image {
    ImageFormat format{ImageFormat::rgba8_unorm};
    std::vector<Mip> mips;
};
struct Instance {
    std::uint32_t primitive{}, source_node{};
    std::array<float, 16> transform{};
};
struct Light {
    std::uint32_t type{}; // Directional=0, point=1, spot=2; original candela/lux units.
    std::array<float, 3> color{1, 1, 1};
    float intensity{1}, range{}, inner_angle{}, outer_angle{};
};
struct LightInstance {
    std::uint32_t light{}, source_node{};
    std::array<float, 16> transform{};
};
// Published only as an owning immutable pin. No glTF, JSON or KTX decoder is linked.
struct Scene {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::vector<Primitive> primitives;
    std::vector<Material> materials;
    std::vector<Texture> textures;
    std::vector<Sampler> samplers;
    std::vector<Image> images;
    std::vector<Instance> instances;
    std::vector<Light> lights;
    std::vector<LightInstance> light_instances;
    std::string provenance;
    Bounds bounds;
    std::uint32_t checksum{}; // CRC32 corruption detection, not identity/authentication.
};
enum class Issue : std::uint8_t { invalid, unsupported, capacity, cancelled, io };
struct Failure {
    Issue issue;
    std::string message;
};
using PackagePin = std::shared_ptr<const Scene>;
using PackageResult = std::variant<PackagePin, Failure>;
// Borrow input only during the call; returned vectors/strings/mip bytes own storage.
[[nodiscard]] PackageResult load_package(std::span<const std::byte> bytes,
                                         const std::stop_token &stop = {});
[[nodiscard]] PackageResult load_package_file(const std::filesystem::path &path,
                                              const std::stop_token &stop = {});
} // namespace ngen::assets
