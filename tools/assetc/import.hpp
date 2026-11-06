#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <stop_token>
#include <string>
#include <variant>
#include <vector>

namespace ngen::assetc {
inline constexpr std::size_t source_byte_limit = 256ULL * 1024 * 1024;
inline constexpr std::size_t vertex_limit = 4ULL * 1024 * 1024;
inline constexpr std::size_t index_limit = 12ULL * 1024 * 1024;
inline constexpr std::size_t primitive_limit = 4096;
inline constexpr std::size_t tangent_repair_limit = 4096;

struct Vertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
    std::array<float, 2> uv{};
    std::array<float, 2> uv1{};
    std::array<float, 4> tangent{};
};
struct Bounds {
    std::array<float, 3> minimum{};
    std::array<float, 3> maximum{};
};
struct Primitive {
    std::vector<Vertex> vertices;
    std::vector<std::uint32_t> indices;
    std::uint32_t material{UINT32_MAX};
    Bounds bounds;
};
struct Mesh {
    std::uint32_t first_primitive{};
    std::uint32_t primitive_count{};
};
struct Instance {
    std::uint32_t node{};
    std::uint32_t primitive{};
    std::array<float, 16> transform{}; // Column-major, original glTF metres/right-handed axes.
};
struct Image {
    std::uint32_t width{};
    std::uint32_t height{};
    // RGBA8 channel bytes, not color-managed by the PNG's gAMA/sRGB metadata.
    // A material texture slot determines sRGB versus linear interpretation.
    std::vector<std::byte> pixels;
};
enum class Filter : std::uint16_t {
    automatic = 0,
    nearest = 9728,
    linear = 9729,
    nearest_mipmap_nearest = 9984,
    linear_mipmap_nearest = 9985,
    nearest_mipmap_linear = 9986,
    linear_mipmap_linear = 9987
};
enum class Wrap : std::uint16_t { repeat = 10497, clamp = 33071, mirrored_repeat = 33648 };
struct Sampler {
    Filter min{Filter::automatic};
    Filter mag{Filter::automatic};
    Wrap s{Wrap::repeat};
    Wrap t{Wrap::repeat};
};
struct Texture {
    std::uint32_t image{};
    std::uint32_t sampler{UINT32_MAX};
};
enum class AlphaMode : std::uint8_t { opaque, mask, blend };
struct TextureReference {
    std::uint32_t texture{UINT32_MAX};
    std::uint32_t uv_set{};
};
struct Material {
    std::string name;
    std::array<float, 4> base_color{1, 1, 1, 1};
    std::array<float, 3> emissive{};
    float emissive_strength{1};
    float metallic{1};
    float roughness{1};
    float normal_scale{1};
    float occlusion_strength{1};
    float alpha_cutoff{0.5F};
    AlphaMode alpha{AlphaMode::opaque};
    bool double_sided{};
    TextureReference base_color_map;
    TextureReference metallic_roughness_map;
    TextureReference normal_map;
    TextureReference occlusion_map;
    TextureReference emissive_map;
};
enum class LightType : std::uint8_t { directional, point, spot };
struct Light {
    LightType type{LightType::point};
    std::array<float, 3> color{1, 1, 1};
    float intensity{1}; // Candela for point/spot; lux for directional.
    float range{};      // Zero represents an omitted/infinite range.
    float inner_angle{};
    float outer_angle{};
};
struct LightInstance {
    std::uint32_t node{};
    std::uint32_t light{};
    std::array<float, 16> transform{};
};
struct TangentStatistics {
    std::size_t invalid_source_vertices{};
    std::size_t missing_source_vertices{};
    std::size_t degenerate_uv_triangles{};
    std::size_t rebuilt_primitives{};
    std::size_t rebuilt_corners{};
    std::size_t derivative_fallback_corners{};
    std::size_t degenerate_projection_corners{};
};
struct Scene {
    std::vector<Primitive> primitives;
    std::vector<Mesh> meshes;
    std::vector<Instance> instances;
    std::vector<Material> materials;
    std::vector<Image> images;
    std::vector<Texture> textures;
    std::vector<Sampler> samplers;
    std::vector<Light> lights;
    std::vector<LightInstance> light_instances;
    std::size_t source_nodes{};
    std::size_t source_vertices{};
    TangentStatistics tangents;
    Bounds bounds; // Conservative world-space transformed primitive boxes.
};
enum class ImportIssue : std::uint8_t { invalid, unsupported, capacity, cancelled };
struct ImportFailure {
    ImportIssue issue;
    std::string message;
};
using ImportResult = std::variant<Scene, ImportFailure>;
struct ImportOptions {
    bool rebuild_tangents{false}; // Explicit canonical-cooking transform; strict by default.
};
// Tool-only, no I/O. Borrows bytes during this call; every returned value owns
// its storage. Cancellation is cooperative; a library parse is not interruptible.
[[nodiscard]] ImportResult import_glb(std::span<const std::byte> bytes, ImportOptions options = {},
                                      const std::stop_token &stop = {});
} // namespace ngen::assetc
