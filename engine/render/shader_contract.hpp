#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ngen::render::shader {

using Float4 = std::array<float, 4>;

// Column-major memory; vectors multiply on the right. World positions here are
// already camera-relative floats, never astronomical coordinates narrowed directly.
struct alignas(16) Matrix {
    std::array<float, 16> columns{};
};

struct alignas(16) FrameConstants {
    Matrix viewProjection;
    Matrix model;
    Matrix normalMatrix;
    Matrix lightViewProjection;
    Float4 cameraPosition{};
    Float4 keyDirectionIntensity{};
    Float4 keyColor{};
};

struct alignas(16) LocalLight {
    Float4 positionRange{};
    Float4 colorIntensity{};
};

inline constexpr std::size_t local_light_capacity = 16;
struct alignas(16) LightConstants {
    std::array<LocalLight, local_light_capacity> lights{};
    std::uint32_t count{};
};

struct alignas(16) MaterialConstants {
    Float4 baseColor{1, 1, 1, 1};
    Float4 emissiveAlphaCutoff{0, 0, 0, 0.5F};
    Float4 metallicRoughnessNormalOcclusion{1, 1, 1, 1};
    // [0]: opaque=0, mask=1, blend=2; [1]: UV1 bits in package map order
    // (base, metallic/roughness, normal, occlusion, emissive); [2] bit0: no normal
    // texture. All remaining bits, including [3], are reserved.
    std::array<std::uint32_t, 4> flags{};
};

struct alignas(16) PostConstants {
    Float4 exposureFxaaTexel{1, 1, 1, 1};
};

struct alignas(16) CullConstants {
    std::array<Float4, 6> planes{};
    std::uint32_t count{};
    std::array<std::uint32_t, 3> reserved{};
};

struct Vertex {
    std::array<float, 3> position{};
    std::array<float, 3> normal{};
    std::array<float, 2> uv{};
    std::array<float, 2> uv1{};
    Float4 tangent{};
};

static_assert(sizeof(float) == 4);
static_assert(sizeof(Matrix) == 64);
static_assert(sizeof(FrameConstants) == 304);
static_assert(sizeof(LocalLight) == 32);
static_assert(sizeof(LightConstants) == 528);
static_assert(sizeof(MaterialConstants) == 64);
static_assert(sizeof(CullConstants) == 112);
static_assert(offsetof(CullConstants, count) == 96 && offsetof(CullConstants, reserved) == 100);
static_assert(sizeof(PostConstants) == 16);
static_assert(sizeof(Vertex) == 56);
static_assert(std::is_standard_layout_v<FrameConstants> &&
              std::is_trivially_copyable_v<FrameConstants>);
static_assert(std::is_standard_layout_v<LightConstants> &&
              std::is_trivially_copyable_v<LightConstants>);
static_assert(std::is_standard_layout_v<MaterialConstants> &&
              std::is_trivially_copyable_v<MaterialConstants>);
static_assert(std::is_standard_layout_v<PostConstants> &&
              std::is_trivially_copyable_v<PostConstants>);
static_assert(std::is_standard_layout_v<CullConstants> &&
              std::is_trivially_copyable_v<CullConstants>);
static_assert(std::is_standard_layout_v<Vertex> && std::is_trivially_copyable_v<Vertex>);

} // namespace ngen::render::shader
