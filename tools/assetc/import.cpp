#include "assetc/import.hpp"
#include "assetc/metadata.hpp"
#include "assetc/png.hpp"
#include "assetc/tangents.hpp"
#include <algorithm>
#include <cmath>
#include <exception>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <numbers>
#include <numeric>

namespace ngen::assetc {
namespace {
struct Rejected : std::exception {
    ImportFailure failure;
    explicit Rejected(ImportFailure value) : failure(std::move(value)) {}
    const char *what() const noexcept override { return failure.message.c_str(); }
};
[[noreturn]] void reject(ImportIssue issue, const char *message) {
    throw Rejected{{issue, message}};
}
void cancelled(const std::stop_token &stop) {
    if (stop.stop_requested())
        reject(ImportIssue::cancelled, "asset import cancelled");
}
std::uint32_t u32(std::span<const std::byte> bytes, std::size_t offset) {
    if (offset > bytes.size() || bytes.size() - offset < 4)
        reject(ImportIssue::invalid, "truncated GLB container");
    std::uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i)
        value |= std::to_integer<std::uint32_t>(bytes[offset + i]) << (i * 8);
    return value;
}
void container(std::span<const std::byte> bytes) {
    if (bytes.size() > source_byte_limit)
        reject(ImportIssue::capacity, "source byte limit exceeded");
    if (bytes.size() < 28 || u32(bytes, 0) != 0x46546c67 || u32(bytes, 4) != 2 ||
        u32(bytes, 8) != bytes.size())
        reject(ImportIssue::invalid, "expected an exact glTF 2.0 GLB container");
    const auto json_bytes = u32(bytes, 12);
    if (json_bytes > 4U * 1024 * 1024)
        reject(ImportIssue::capacity, "GLB JSON byte limit exceeded");
    if (json_bytes == 0 || json_bytes % 4 != 0 || u32(bytes, 16) != 0x4e4f534a ||
        json_bytes > bytes.size() - 28)
        reject(ImportIssue::invalid, "invalid GLB JSON chunk");
    const std::size_t binary_offset = 20 + std::size_t{json_bytes};
    const auto binary_bytes = u32(bytes, binary_offset);
    if (u32(bytes, binary_offset + 4) != 0x004e4942 || binary_bytes % 4 != 0 ||
        binary_bytes != bytes.size() - binary_offset - 8)
        reject(ImportIssue::invalid, "expected one embedded GLB binary chunk");
}
void validate_buffers(const fastgltf::Asset &asset) {
    if (asset.buffers.size() != 1)
        reject(ImportIssue::unsupported, "only one embedded GLB buffer is supported");
    const auto &buffer = asset.buffers[0];
    const auto *array = std::get_if<fastgltf::sources::Array>(&buffer.data);
    if (!array)
        reject(ImportIssue::unsupported, "external buffer URIs are not allowed");
    if (buffer.byteLength > source_byte_limit || buffer.byteLength > array->bytes.size())
        reject(ImportIssue::invalid, "declared buffer exceeds embedded bytes");
    for (const auto &view : asset.bufferViews) {
        if (view.bufferIndex != 0 || view.byteOffset > buffer.byteLength ||
            view.byteLength > buffer.byteLength - view.byteOffset)
            reject(ImportIssue::invalid, "buffer view range exceeds embedded bytes");
        if (view.meshoptCompression)
            reject(ImportIssue::unsupported, "compressed source buffer views are not supported");
    }
    for (const auto &accessor : asset.accessors) {
        if (accessor.sparse)
            reject(ImportIssue::unsupported, "sparse source accessors are not supported");
        if (!accessor.bufferViewIndex || *accessor.bufferViewIndex >= asset.bufferViews.size())
            reject(ImportIssue::invalid, "accessor requires a valid buffer view");
        const auto &view = asset.bufferViews[*accessor.bufferViewIndex];
        const auto element = fastgltf::getElementByteSize(accessor.type, accessor.componentType);
        const auto component = fastgltf::getComponentByteSize(accessor.componentType);
        const auto stride = view.byteStride.value_or(element);
        if (component == 0 || element == 0 || stride < element || stride % component != 0 ||
            accessor.byteOffset % component != 0 || view.byteOffset % component != 0 ||
            accessor.byteOffset > view.byteLength ||
            element > view.byteLength - accessor.byteOffset || accessor.count == 0 ||
            accessor.count - 1 > (view.byteLength - accessor.byteOffset - element) / stride)
            reject(ImportIssue::invalid, "accessor range or stride exceeds buffer view");
    }
}
const fastgltf::Accessor &attribute(const fastgltf::Asset &asset,
                                    const fastgltf::Primitive &primitive, std::string_view name,
                                    fastgltf::AccessorType type) {
    const auto found = primitive.findAttribute(name);
    if (found == primitive.attributes.end())
        reject(ImportIssue::unsupported, "primitive requires POSITION, NORMAL, UV0 and TANGENT");
    if (found->accessorIndex >= asset.accessors.size())
        reject(ImportIssue::invalid, "attribute references an invalid accessor");
    const auto &accessor = asset.accessors[found->accessorIndex];
    if (accessor.type != type || accessor.componentType != fastgltf::ComponentType::Float ||
        accessor.normalized)
        reject(ImportIssue::unsupported, "vertex attributes require unnormalized float32 vectors");
    return accessor;
}
template <std::size_t N> auto values(const fastgltf::math::vec<float, N> &value) {
    std::array<float, N> result{};
    for (std::size_t i = 0; i < N; ++i) {
        if (!std::isfinite(value[i]))
            reject(ImportIssue::invalid, "vertex attribute is not finite");
        result[i] = value[i];
    }
    return result;
}
std::array<float, 16> matrix_values(const fastgltf::math::fmat4x4 &matrix) {
    std::array<float, 16> result{};
    for (std::size_t c = 0; c < 4; ++c)
        for (std::size_t r = 0; r < 4; ++r) {
            if (!std::isfinite(matrix[c][r]))
                reject(ImportIssue::invalid, "node world transform is not finite");
            result[c * 4 + r] = matrix[c][r];
        }
    if (result[3] != 0 || result[7] != 0 || result[11] != 0 || result[15] != 1)
        reject(ImportIssue::invalid, "node transform must be affine");
    const auto at = [&](std::size_t column, std::size_t row) {
        return static_cast<double>(matrix[column][row]);
    };
    const double determinant = at(0, 0) * (at(1, 1) * at(2, 2) - at(2, 1) * at(1, 2)) -
                               at(1, 0) * (at(0, 1) * at(2, 2) - at(2, 1) * at(0, 2)) +
                               at(2, 0) * (at(0, 1) * at(1, 2) - at(1, 1) * at(0, 2));
    if (determinant == 0)
        reject(ImportIssue::unsupported,
               "singular node transforms are outside the static rendering subset");
    return result;
}
bool unit_range(float value) { return std::isfinite(value) && value >= 0 && value <= 1; }
TextureReference texture_reference(const fastgltf::Asset &asset,
                                   const fastgltf::TextureInfo &source) {
    if (source.textureIndex >= asset.textures.size())
        reject(ImportIssue::invalid, "material references an invalid texture");
    if (source.texCoordIndex > 1 || source.transform)
        reject(ImportIssue::unsupported,
               "only UV0/UV1 without texture-transform extensions are supported");
    return {static_cast<std::uint32_t>(source.textureIndex),
            static_cast<std::uint32_t>(source.texCoordIndex)};
}
void images(const fastgltf::Asset &asset, Scene &scene, const std::stop_token &stop) {
    if (asset.images.size() > 64)
        reject(ImportIssue::capacity, "image limit exceeded");
    std::size_t admitted = 0;
    constexpr std::size_t decoded_limit = 512ULL * 1024 * 1024;
    for (const auto &source : asset.images) {
        cancelled(stop);
        const auto *view_source = std::get_if<fastgltf::sources::BufferView>(&source.data);
        if (!view_source || view_source->mimeType != fastgltf::MimeType::PNG)
            reject(ImportIssue::unsupported,
                   "images must be PNG data in embedded buffer views; no image URIs");
        if (view_source->bufferViewIndex >= asset.bufferViews.size())
            reject(ImportIssue::invalid, "image references an invalid buffer view");
        const auto &view = asset.bufferViews[view_source->bufferViewIndex];
        const auto &data = std::get<fastgltf::sources::Array>(asset.buffers[0].data);
        const auto bytes = std::span<const std::byte>{data.bytes.data(), data.bytes.size()}.subspan(
            view.byteOffset, view.byteLength);
        auto decoded = decode_png(bytes, decoded_limit - admitted);
        if (const auto *failure = std::get_if<ImportFailure>(&decoded))
            throw Rejected{*failure};
        auto &image = std::get<Image>(decoded);
        admitted += image.pixels.size();
        scene.images.push_back(std::move(image));
    }
}
Filter filtering(fastgltf::Optional<fastgltf::Filter> value, bool magnification) {
    if (!value)
        return Filter::automatic;
    switch (*value) {
    case fastgltf::Filter::Nearest:
    case fastgltf::Filter::Linear:
        break;
    case fastgltf::Filter::NearestMipMapNearest:
    case fastgltf::Filter::LinearMipMapNearest:
    case fastgltf::Filter::NearestMipMapLinear:
    case fastgltf::Filter::LinearMipMapLinear:
        if (magnification)
            reject(ImportIssue::invalid, "magnification filtering cannot use mipmaps");
        break;
    default:
        reject(ImportIssue::invalid, "unsupported sampler filter value");
    }
    return static_cast<Filter>(*value);
}
Wrap wrapping(fastgltf::Wrap value) {
    switch (value) {
    case fastgltf::Wrap::Repeat:
    case fastgltf::Wrap::ClampToEdge:
    case fastgltf::Wrap::MirroredRepeat:
        return static_cast<Wrap>(value);
    default:
        reject(ImportIssue::invalid, "unsupported sampler wrap value");
    }
}
void textures(const fastgltf::Asset &asset, Scene &scene) {
    if (asset.samplers.size() > 64 || asset.textures.size() > 256)
        reject(ImportIssue::capacity, "sampler or texture limit exceeded");
    for (const auto &source : asset.samplers)
        scene.samplers.push_back({filtering(source.minFilter, false),
                                  filtering(source.magFilter, true), wrapping(source.wrapS),
                                  wrapping(source.wrapT)});
    for (const auto &source : asset.textures) {
        if (!source.imageIndex || *source.imageIndex >= scene.images.size())
            reject(ImportIssue::invalid, "texture requires a valid embedded image");
        Texture texture;
        texture.image = static_cast<std::uint32_t>(*source.imageIndex);
        if (source.samplerIndex) {
            if (*source.samplerIndex >= scene.samplers.size())
                reject(ImportIssue::invalid, "texture references an invalid sampler");
            texture.sampler = static_cast<std::uint32_t>(*source.samplerIndex);
        }
        scene.textures.push_back(texture);
    }
}
void materials(const fastgltf::Asset &asset, Scene &scene) {
    if (asset.materials.size() > 256)
        reject(ImportIssue::capacity, "material limit exceeded");
    for (const auto &source : asset.materials) {
        if (source.name.size() > 256)
            reject(ImportIssue::capacity, "material name byte limit exceeded");
        Material material;
        material.name.assign(source.name.data(), source.name.size());
        material.base_color = values(source.pbrData.baseColorFactor);
        material.emissive = values(source.emissiveFactor);
        material.emissive_strength = source.emissiveStrength;
        material.metallic = source.pbrData.metallicFactor;
        material.roughness = source.pbrData.roughnessFactor;
        material.alpha_cutoff = source.alphaCutoff;
        material.double_sided = source.doubleSided;
        switch (source.alphaMode) {
        case fastgltf::AlphaMode::Opaque:
            material.alpha = AlphaMode::opaque;
            break;
        case fastgltf::AlphaMode::Mask:
            material.alpha = AlphaMode::mask;
            break;
        case fastgltf::AlphaMode::Blend:
            material.alpha = AlphaMode::blend;
            break;
        default:
            reject(ImportIssue::invalid, "invalid material alpha mode");
        }
        if (source.pbrData.baseColorTexture)
            material.base_color_map = texture_reference(asset, *source.pbrData.baseColorTexture);
        if (source.pbrData.metallicRoughnessTexture)
            material.metallic_roughness_map =
                texture_reference(asset, *source.pbrData.metallicRoughnessTexture);
        if (source.normalTexture) {
            material.normal_map = texture_reference(asset, *source.normalTexture);
            material.normal_scale = source.normalTexture->scale;
        }
        if (source.occlusionTexture) {
            material.occlusion_map = texture_reference(asset, *source.occlusionTexture);
            material.occlusion_strength = source.occlusionTexture->strength;
        }
        if (source.emissiveTexture)
            material.emissive_map = texture_reference(asset, *source.emissiveTexture);
        if (!std::all_of(material.base_color.begin(), material.base_color.end(), unit_range) ||
            !std::all_of(material.emissive.begin(), material.emissive.end(), unit_range) ||
            !std::isfinite(material.emissive_strength) || material.emissive_strength < 0 ||
            !unit_range(material.metallic) || !unit_range(material.roughness) ||
            !unit_range(material.occlusion_strength) || !std::isfinite(material.normal_scale) ||
            !std::isfinite(material.alpha_cutoff) || material.alpha_cutoff < 0)
            reject(ImportIssue::invalid, "material factors are outside glTF limits");
        for (float factor : material.emissive)
            if (!std::isfinite(factor * material.emissive_strength))
                reject(ImportIssue::invalid, "material emission overflows float32");
        scene.materials.push_back(std::move(material));
    }
}
void light_definitions(const fastgltf::Asset &asset, Scene &scene) {
    if (asset.lights.size() > 4096)
        reject(ImportIssue::capacity, "light definition limit exceeded");
    for (const auto &source : asset.lights) {
        Light light;
        switch (source.type) {
        case fastgltf::LightType::Directional:
            light.type = LightType::directional;
            break;
        case fastgltf::LightType::Point:
            light.type = LightType::point;
            break;
        case fastgltf::LightType::Spot:
            light.type = LightType::spot;
            break;
        default:
            reject(ImportIssue::invalid, "invalid punctual light type");
        }
        light.color = values(source.color);
        light.intensity = source.intensity;
        light.range = source.range.value_or(0);
        light.inner_angle = source.innerConeAngle.value_or(0);
        light.outer_angle = source.outerConeAngle.value_or(std::numbers::pi_v<float> / 4);
        if (!std::isfinite(light.intensity) || light.intensity < 0 || !std::isfinite(light.range) ||
            (source.range && light.range <= 0) || !std::isfinite(light.inner_angle) ||
            !std::isfinite(light.outer_angle) || light.inner_angle < 0 ||
            light.outer_angle <= light.inner_angle ||
            light.outer_angle > std::numbers::pi_v<float> / 2 ||
            std::any_of(light.color.begin(), light.color.end(),
                        [](float value) { return value < 0 || value > 1; }))
            reject(ImportIssue::invalid, "punctual light values are outside glTF limits");
        scene.lights.push_back(light);
    }
}
void expand(Bounds &bounds, const std::array<float, 3> &point) {
    for (std::size_t axis = 0; axis < 3; ++axis) {
        bounds.minimum[axis] = std::min(bounds.minimum[axis], point[axis]);
        bounds.maximum[axis] = std::max(bounds.maximum[axis], point[axis]);
    }
}
void instance_bounds(Scene &scene, const Instance &instance) {
    const auto &local = scene.primitives[instance.primitive].bounds;
    for (unsigned corner = 0; corner < 8; ++corner) {
        std::array<float, 3> point{};
        for (std::size_t row = 0; row < 3; ++row) {
            point[row] = instance.transform[12 + row];
            for (std::size_t column = 0; column < 3; ++column)
                point[row] += instance.transform[column * 4 + row] * ((corner & (1U << column)) != 0
                                                                          ? local.maximum[column]
                                                                          : local.minimum[column]);
            if (!std::isfinite(point[row]))
                reject(ImportIssue::invalid, "transformed geometry bounds are not finite");
        }
        if (scene.instances.empty() && corner == 0)
            scene.bounds = {point, point};
        expand(scene.bounds, point);
    }
}
void instances(const fastgltf::Asset &asset, Scene &scene, const std::stop_token &stop) {
    constexpr std::size_t node_limit = 4096;
    if (asset.nodes.size() > node_limit || asset.scenes.empty() || asset.scenes.size() > 16)
        reject(ImportIssue::capacity, "scene or node limit exceeded");
    if (!asset.defaultScene && asset.scenes.size() != 1)
        reject(ImportIssue::unsupported, "multiple scenes require an explicit default scene");
    const auto selected = asset.defaultScene.value_or(0);
    if (selected >= asset.scenes.size())
        reject(ImportIssue::invalid, "default scene index is invalid");
    scene.source_nodes = asset.nodes.size();
    std::vector<std::size_t> parents(asset.nodes.size(), node_limit);
    for (std::size_t i = 0; i < asset.nodes.size(); ++i) {
        const auto &node = asset.nodes[i];
        if (!node.weights.empty() || !node.instancingAttributes.empty())
            reject(ImportIssue::unsupported,
                   "morph weights or GPU instance extensions are not supported");
        for (auto child : node.children) {
            if (child >= parents.size() || parents[child] != node_limit || child == i)
                reject(ImportIssue::invalid, "node hierarchy has invalid or repeated children");
            parents[child] = i;
        }
    }
    std::vector<bool> active(asset.nodes.size(), false);
    for (auto index : asset.scenes[selected].nodeIndices) {
        if (index >= parents.size() || parents[index] != node_limit || active[index])
            reject(ImportIssue::invalid, "scene roots must be distinct hierarchy roots");
        active[index] = true;
    }
    std::vector<std::size_t> ready;
    ready.reserve(asset.nodes.size());
    std::vector<fastgltf::math::fmat4x4> world(asset.nodes.size());
    for (std::size_t i = 0; i < parents.size(); ++i)
        if (parents[i] == node_limit)
            ready.push_back(i);
    for (std::size_t cursor = 0; cursor < ready.size(); ++cursor) {
        cancelled(stop);
        const auto index = ready[cursor];
        const auto &node = asset.nodes[index];
        if (const auto *trs = std::get_if<fastgltf::TRS>(&node.transform)) {
            double squared_length = 0;
            for (std::size_t i = 0; i < 4; ++i) {
                const float value = trs->rotation[i];
                if (!std::isfinite(value))
                    reject(ImportIssue::invalid, "node rotation quaternion is not finite");
                squared_length += static_cast<double>(value) * value;
            }
            if (std::abs(squared_length - 1) > 0.002)
                reject(ImportIssue::invalid, "node rotation quaternion must be unit length");
        }
        const auto base =
            parents[index] == node_limit ? fastgltf::math::fmat4x4() : world[parents[index]];
        world[index] = fastgltf::getTransformMatrix(node, base);
        const auto transform = matrix_values(world[index]);
        if (active[index] && node.meshIndex) {
            if (*node.meshIndex >= scene.meshes.size())
                reject(ImportIssue::invalid, "node references an invalid mesh");
            const auto &mesh = scene.meshes[*node.meshIndex];
            for (std::uint32_t p = 0; p < mesh.primitive_count; ++p) {
                if (scene.instances.size() == primitive_limit)
                    reject(ImportIssue::capacity, "mesh instance limit exceeded");
                Instance instance;
                instance.node = static_cast<std::uint32_t>(index);
                instance.primitive = mesh.first_primitive + p;
                instance.transform = transform;
                instance_bounds(scene, instance);
                scene.instances.push_back(instance);
            }
        }
        if (active[index] && node.lightIndex) {
            if (*node.lightIndex >= scene.lights.size())
                reject(ImportIssue::invalid, "node references an invalid light");
            if (scene.light_instances.size() == node_limit)
                reject(ImportIssue::capacity, "light instance limit exceeded");
            scene.light_instances.push_back({static_cast<std::uint32_t>(index),
                                             static_cast<std::uint32_t>(*node.lightIndex),
                                             transform});
        }
        for (auto child : node.children) {
            active[child] = active[index];
            ready.push_back(child);
        }
    }
    if (ready.size() != asset.nodes.size())
        reject(ImportIssue::invalid, "node hierarchy contains a cycle");
}
Scene extract(const fastgltf::Asset &asset, ImportOptions options, const std::stop_token &stop) {
    Scene scene;
    images(asset, scene, stop);
    textures(asset, scene);
    materials(asset, scene);
    std::size_t vertices = 0;
    std::size_t indices = 0;
    if (asset.meshes.size() > primitive_limit)
        reject(ImportIssue::capacity, "mesh limit exceeded");
    for (const auto &mesh : asset.meshes) {
        if (mesh.primitives.size() > primitive_limit - scene.primitives.size())
            reject(ImportIssue::capacity, "primitive limit exceeded");
        scene.meshes.push_back({static_cast<std::uint32_t>(scene.primitives.size()),
                                static_cast<std::uint32_t>(mesh.primitives.size())});
        for (const auto &source : mesh.primitives) {
            cancelled(stop);
            if (scene.primitives.size() == primitive_limit)
                reject(ImportIssue::capacity, "primitive limit exceeded");
            if (source.type != fastgltf::PrimitiveType::Triangles || !source.targets.empty() ||
                source.dracoCompression)
                reject(ImportIssue::unsupported,
                       "only static uncompressed triangles are supported");
            for (const auto &item : source.attributes)
                if (item.name != "POSITION" && item.name != "NORMAL" && item.name != "TANGENT" &&
                    item.name != "TEXCOORD_0" && item.name != "TEXCOORD_1")
                    reject(ImportIssue::unsupported,
                           "vertex attributes outside POSITION/NORMAL/TANGENT/UV0/UV1 are not "
                           "supported");
            const auto &positions =
                attribute(asset, source, "POSITION", fastgltf::AccessorType::Vec3);
            const auto &normals = attribute(asset, source, "NORMAL", fastgltf::AccessorType::Vec3);
            const auto &uvs = attribute(asset, source, "TEXCOORD_0", fastgltf::AccessorType::Vec2);
            const fastgltf::Accessor *tangents = nullptr;
            if (source.findAttribute("TANGENT") != source.attributes.end())
                tangents = &attribute(asset, source, "TANGENT", fastgltf::AccessorType::Vec4);
            else if (!options.rebuild_tangents)
                reject(ImportIssue::unsupported,
                       "missing tangents require explicit cooking regeneration");
            if (positions.count > vertex_limit - scene.source_vertices)
                reject(ImportIssue::capacity, "source vertex limit exceeded");
            if (normals.count != positions.count || uvs.count != positions.count ||
                (tangents && tangents->count != positions.count))
                reject(ImportIssue::invalid, "primitive attribute counts do not match");
            Primitive primitive;
            if (source.materialIndex) {
                if (*source.materialIndex >= scene.materials.size())
                    reject(ImportIssue::invalid, "primitive references an invalid material");
                primitive.material = static_cast<std::uint32_t>(*source.materialIndex);
                const auto &material = scene.materials[primitive.material];
                const std::array references{material.base_color_map,
                                            material.metallic_roughness_map, material.normal_map,
                                            material.occlusion_map, material.emissive_map};
                for (const auto &reference : references)
                    if (reference.texture != UINT32_MAX && reference.uv_set == 1 &&
                        source.findAttribute("TEXCOORD_1") == source.attributes.end())
                        reject(ImportIssue::invalid, "material references missing UV1 data");
            }
            primitive.vertices.resize(positions.count);
            scene.source_vertices += positions.count;
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                asset, positions, [&](auto value, std::size_t index) {
                    if (index % 1024 == 0)
                        cancelled(stop);
                    primitive.vertices[index].position = values(value);
                    if (index == 0)
                        primitive.bounds = {primitive.vertices[index].position,
                                            primitive.vertices[index].position};
                    expand(primitive.bounds, primitive.vertices[index].position);
                });
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
                asset, normals, [&](auto value, std::size_t index) {
                    primitive.vertices[index].normal = values(value);
                });
            fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                asset, uvs, [&](auto value, std::size_t index) {
                    primitive.vertices[index].uv = values(value);
                });
            if (tangents)
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(
                    asset, *tangents, [&](auto value, std::size_t index) {
                        primitive.vertices[index].tangent = values(value);
                    });
            else
                scene.tangents.missing_source_vertices += positions.count;
            if (source.findAttribute("TEXCOORD_1") != source.attributes.end()) {
                const auto &secondary =
                    attribute(asset, source, "TEXCOORD_1", fastgltf::AccessorType::Vec2);
                if (secondary.count != positions.count)
                    reject(ImportIssue::invalid, "secondary UV count does not match positions");
                fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
                    asset, secondary, [&](auto value, std::size_t index) {
                        primitive.vertices[index].uv1 = values(value);
                    });
            }
            for (const auto &vertex : primitive.vertices) {
                const auto &n = vertex.normal;
                const auto normal_length = std::inner_product(n.begin(), n.end(), n.begin(), 0.0F);
                if (std::abs(normal_length - 1) > 0.002F)
                    reject(ImportIssue::invalid, "normal vector is not unit length");
                if (tangents && !valid_frame(vertex)) {
                    ++scene.tangents.invalid_source_vertices;
                    if (!options.rebuild_tangents)
                        reject(ImportIssue::invalid, "normal/tangent frame is not orthonormal");
                }
            }
            if (!source.indicesAccessor) {
                if (positions.count > index_limit - indices)
                    reject(ImportIssue::capacity, "index limit exceeded");
                if (positions.count % 3 != 0)
                    reject(ImportIssue::invalid, "unindexed triangles require vertex triples");
                primitive.indices.resize(positions.count);
                std::iota(primitive.indices.begin(), primitive.indices.end(), std::uint32_t{0});
            } else {
                const auto &accessor = asset.accessors[*source.indicesAccessor];
                if (accessor.count > index_limit - indices)
                    reject(ImportIssue::capacity, "index limit exceeded");
                if (accessor.type != fastgltf::AccessorType::Scalar || accessor.normalized ||
                    accessor.count % 3 != 0 ||
                    (accessor.componentType != fastgltf::ComponentType::UnsignedByte &&
                     accessor.componentType != fastgltf::ComponentType::UnsignedShort &&
                     accessor.componentType != fastgltf::ComponentType::UnsignedInt))
                    reject(ImportIssue::invalid,
                           "triangle indices require unsigned scalar triples");
                primitive.indices.reserve(accessor.count);
                fastgltf::iterateAccessor<std::uint32_t>(asset, accessor, [&](std::uint32_t value) {
                    if (value >= primitive.vertices.size())
                        reject(ImportIssue::invalid, "triangle index exceeds vertex count");
                    primitive.indices.push_back(value);
                });
            }
            indices += primitive.indices.size();
            if (options.rebuild_tangents) {
                cancelled(stop);
                std::uint32_t normal_uv = 0;
                if (primitive.material != UINT32_MAX &&
                    scene.materials[primitive.material].normal_map.texture != UINT32_MAX)
                    normal_uv = scene.materials[primitive.material].normal_map.uv_set;
                if (normal_uv == 1 && source.findAttribute("TEXCOORD_1") == source.attributes.end())
                    reject(ImportIssue::invalid, "normal map references missing UV1 data");
                auto rebuilt = regenerate_tangents(
                    primitive, {normal_uv, vertex_limit - vertices,
                                tangent_repair_limit - scene.tangents.derivative_fallback_corners -
                                    scene.tangents.degenerate_projection_corners});
                if (const auto *failure = std::get_if<ImportFailure>(&rebuilt))
                    throw Rejected{
                        {failure->issue, "mesh " + std::to_string(scene.meshes.size() - 1) +
                                             " primitive " +
                                             std::to_string(scene.primitives.size() -
                                                            scene.meshes.back().first_primitive) +
                                             ": " + failure->message}};
                const auto &statistics = std::get<TangentStatistics>(rebuilt);
                scene.tangents.rebuilt_primitives += statistics.rebuilt_primitives;
                scene.tangents.rebuilt_corners += statistics.rebuilt_corners;
                scene.tangents.derivative_fallback_corners +=
                    statistics.derivative_fallback_corners;
                scene.tangents.degenerate_projection_corners +=
                    statistics.degenerate_projection_corners;
                scene.tangents.degenerate_uv_triangles += statistics.degenerate_uv_triangles;
                cancelled(stop);
            }
            if (primitive.vertices.size() > vertex_limit - vertices)
                reject(ImportIssue::capacity, "output vertex limit exceeded");
            vertices += primitive.vertices.size();
            scene.primitives.push_back(std::move(primitive));
        }
    }
    light_definitions(asset, scene);
    instances(asset, scene, stop);
    cancelled(stop);
    return scene;
}
} // namespace
ImportResult import_glb(std::span<const std::byte> bytes, ImportOptions options,
                        const std::stop_token &stop) {
    try {
        cancelled(stop);
        container(bytes);
        if (auto failure = check_metadata(bytes.subspan(20, u32(bytes, 12))))
            throw Rejected{*failure};
        auto data = fastgltf::GltfDataBuffer::FromBytes(bytes.data(), bytes.size());
        if (data.error() != fastgltf::Error::None)
            return ImportFailure{ImportIssue::invalid,
                                 std::string{fastgltf::getErrorName(data.error())}};
        fastgltf::Parser parser(fastgltf::Extensions::KHR_lights_punctual |
                                fastgltf::Extensions::KHR_materials_emissive_strength);
        auto parsed = parser.loadGltfBinary(data.get(), {}, fastgltf::Options::None);
        if (parsed.error() != fastgltf::Error::None)
            return ImportFailure{ImportIssue::invalid,
                                 std::string{fastgltf::getErrorName(parsed.error())}};
        cancelled(stop);
        const auto &asset = parsed.get();
        if (fastgltf::validate(asset) != fastgltf::Error::None)
            reject(ImportIssue::invalid, "glTF structural validation failed");
        if (!asset.animations.empty() || !asset.skins.empty())
            reject(ImportIssue::unsupported, "animation and skinning are not supported");
        validate_buffers(asset);
        return extract(asset, options, stop);
    } catch (const Rejected &error) {
        return error.failure;
    } catch (const std::bad_alloc &) {
        return ImportFailure{ImportIssue::capacity, "asset import allocation failed"};
    }
}
} // namespace ngen::assetc
