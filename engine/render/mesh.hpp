#pragma once
#include "render/resources.hpp"
#include "render/shader_contract.hpp"
#include <array>
#include <span>

namespace ngen::render {
struct MeshSampler {
    VkFilter min{VK_FILTER_LINEAR}, mag{VK_FILTER_LINEAR};
    VkSamplerMipmapMode mip{VK_SAMPLER_MIPMAP_MODE_LINEAR};
    VkSamplerAddressMode u{VK_SAMPLER_ADDRESS_MODE_REPEAT}, v{VK_SAMPLER_ADDRESS_MODE_REPEAT};
    bool mipmaps{true};
    bool operator==(const MeshSampler &) const = default;
};
enum class MeshFacing : std::uint8_t { both, clockwise, counter_clockwise };
struct MeshDraw {
    shader::FrameConstants frame;
    shader::MaterialConstants material;
    // Package order: base, metallic/roughness, normal, occlusion, emissive.
    std::array<const Image *, 5> maps{};
    std::array<MeshSampler, 5> samplers{};
    std::uint32_t first_index{}, index_count{};
    std::int32_t vertex_offset{};
    MeshFacing facing{MeshFacing::counter_clockwise};
    // Empty means one identity instance; otherwise 1..100 copied transforms.
    // Instance transforms compose inside frame.model/normalMatrix. Facing applies
    // to every instance: use both or separate draws for mixed determinant signs.
    std::span<const shader::InstanceTransform> instances{};
    // Optional paired geometry override for another mesh in this fixed batch.
    const Buffer *vertices{};
    const Buffer *indices{};
};
struct MeshPass {
    // Owner-thread borrowing for this call only. Geometry content is trusted:
    // the caller supplies checked primitive-local indices and valid transforms.
    const Buffer &vertices; // shader::Vertex records.
    const Buffer &indices;  // uint32 indices.
    const Image &color;     // Single-mip RGBA16F color attachment.
    const Image &depth;     // Matching single-mip D32 attachment.
    const Image &shadow;
    const Image &irradiance;
    const Image &prefiltered;
    const Image &brdf;
    shader::LightConstants lights;
    std::span<const MeshDraw> draws; // 1..256, copied before return; no retained CPU borrow.
    TimelineWait wait{};
    // Generate shadow depth from opaque/masked draws before the color batch.
    // Requires a distinct sampled D32 depth attachment; blend draws do not cast.
    bool generate_shadow{false};
    std::array<std::byte, 15> shadow_padding{};
};
} // namespace ngen::render
