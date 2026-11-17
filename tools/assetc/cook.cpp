#include "assetc/cook.hpp"
#include <algorithm>
#include <cmath>
#include <exception>
#include <meshoptimizer.h>
#include <type_traits>

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
void check_stop(const std::stop_token &stop) {
    if (stop.stop_requested())
        reject(ImportIssue::cancelled, "scene cooking cancelled");
}
const std::array<double, 256> &linear_samples() {
    static const auto table = [] {
        std::array<double, 256> result{};
        for (std::size_t i = 0; i < result.size(); ++i) {
            const double c = static_cast<double>(i) / 255;
            result[i] = c <= .04045 ? c / 12.92 : std::pow((c + .055) / 1.055, 2.4);
        }
        return result;
    }();
    return table;
}
std::byte linear_byte(double value) {
    return static_cast<std::byte>(
        static_cast<unsigned>(std::lround(std::clamp(value, 0.0, 1.0) * 255)));
}
std::byte srgb_byte(double value) {
    value = std::clamp(value, 0.0, 1.0);
    const double encoded =
        value <= .0031308 ? 12.92 * value : 1.055 * std::pow(value, 1 / 2.4) - .055;
    return linear_byte(encoded);
}
bool srgb_role(ImageRole role) {
    return role == ImageRole::base_color || role == ImageRole::alpha_color ||
           role == ImageRole::emissive;
}
assets::Mip reduce_mip(const assets::Mip &source, ImageRole role, const std::stop_token &stop) {
    assets::Mip output{std::max(1U, source.width / 2), std::max(1U, source.height / 2), {}};
    output.pixels.resize(std::size_t{output.width} * output.height * 4);
    const auto &linear = linear_samples();
    const bool srgb = srgb_role(role);
    for (std::uint32_t y = 0; y < output.height; ++y) {
        if (y % 32 == 0)
            check_stop(stop);
        const double top = static_cast<double>(y) * source.height / output.height;
        const double bottom = static_cast<double>(y + 1) * source.height / output.height;
        for (std::uint32_t x = 0; x < output.width; ++x) {
            const double left = static_cast<double>(x) * source.width / output.width;
            const double right = static_cast<double>(x + 1) * source.width / output.width;
            std::array<double, 3> color{};
            double total = 0, alpha = 0;
            for (auto sy = static_cast<std::uint32_t>(std::floor(top));
                 sy < static_cast<std::uint32_t>(std::ceil(bottom)); ++sy)
                for (auto sx = static_cast<std::uint32_t>(std::floor(left));
                     sx < static_cast<std::uint32_t>(std::ceil(right)); ++sx) {
                    const double weight = (std::min(right, static_cast<double>(sx + 1)) -
                                           std::max(left, static_cast<double>(sx))) *
                                          (std::min(bottom, static_cast<double>(sy + 1)) -
                                           std::max(top, static_cast<double>(sy)));
                    const auto index = (std::size_t{sy} * source.width + sx) * 4;
                    const double a = std::to_integer<unsigned>(source.pixels[index + 3]) / 255.0;
                    for (std::size_t c = 0; c < 3; ++c) {
                        const auto sample = std::to_integer<unsigned>(source.pixels[index + c]);
                        const double decoded = role == ImageRole::normal
                                                   ? static_cast<double>(sample) * 2 - 255
                                                   : (srgb ? linear[sample] : sample / 255.0);
                        color[c] += decoded * (role == ImageRole::alpha_color ? a : 1) * weight;
                    }
                    alpha += a * weight;
                    total += weight;
                }
            const auto index = (std::size_t{y} * output.width + x) * 4;
            if (role == ImageRole::normal) {
                const double length =
                    std::sqrt(color[0] * color[0] + color[1] * color[1] + color[2] * color[2]);
                for (std::size_t c = 0; c < 3; ++c) {
                    const double component =
                        length > 1e-12 ? color[c] / length : (c == 2 ? 1.0 : 0.0);
                    output.pixels[index + c] = linear_byte(component * .5 + .5);
                }
                output.pixels[index + 3] = std::byte{255};
            } else {
                const double denominator = role == ImageRole::alpha_color ? alpha : total;
                for (std::size_t c = 0; c < 3; ++c) {
                    const auto sample = denominator > 0 ? color[c] / denominator : 0;
                    output.pixels[index + c] = srgb ? srgb_byte(sample) : linear_byte(sample);
                }
                output.pixels[index + 3] = linear_byte(alpha / total);
            }
        }
    }
    return output;
}
assets::Image cook_image(const Image &source, ImageRole role, const CookSettings &settings,
                         std::size_t allowance, const std::stop_token &stop) {
    check_stop(stop);
    if (source.width == 0 || source.height == 0 || source.width > 8192 || source.height > 8192 ||
        source.pixels.size() != std::size_t{source.width} * source.height * 4)
        reject(ImportIssue::invalid, "invalid decoded image shape for cooking");
    assets::Mip level{source.width, source.height, source.pixels};
    while (level.width > settings.maximum_texture_dimension ||
           level.height > settings.maximum_texture_dimension)
        level = reduce_mip(level, role, stop);
    assets::Image output;
    output.format =
        srgb_role(role) ? assets::ImageFormat::rgba8_srgb : assets::ImageFormat::rgba8_unorm;
    std::size_t used = 0;
    for (;;) {
        if (level.pixels.size() > allowance - used)
            reject(ImportIssue::capacity, "cooked texture byte budget exceeded");
        used += level.pixels.size();
        output.mips.push_back(std::move(level));
        if (output.mips.back().width == 1 && output.mips.back().height == 1)
            break;
        level = reduce_mip(output.mips.back(), role, stop);
    }
    return output;
}
struct DerivedKey {
    std::uint32_t source;
    ImageRole role;
};
} // namespace
CookResult cook_scene(const Scene &source, CookSettings settings, const std::stop_token &stop) {
    if (stop.stop_requested())
        return ImportFailure{ImportIssue::cancelled, "scene cooking cancelled"};
    if (settings.maximum_texture_dimension == 0 || settings.maximum_texture_dimension > 8192)
        return ImportFailure{ImportIssue::invalid, "invalid cooker texture dimension limit"};
    static_assert(std::is_same_v<std::uint32_t, unsigned int>);
    try {
        CookedScene cooked;
        std::vector<DerivedKey> image_keys, texture_keys;
        const auto texture = [&](std::uint32_t index, ImageRole role) {
            for (std::size_t i = 0; i < texture_keys.size(); ++i)
                if (texture_keys[i].source == index && texture_keys[i].role == role)
                    return static_cast<std::uint32_t>(i);
            if (index >= source.textures.size() ||
                source.textures[index].image >= source.images.size())
                reject(ImportIssue::invalid, "invalid source texture reference during cooking");
            const auto &binding = source.textures[index];
            auto image = static_cast<std::uint32_t>(image_keys.size());
            for (std::size_t i = 0; i < image_keys.size(); ++i)
                if (image_keys[i].source == binding.image && image_keys[i].role == role) {
                    image = static_cast<std::uint32_t>(i);
                    break;
                }
            if (image == image_keys.size()) {
                if (image_keys.size() == 256)
                    reject(ImportIssue::capacity, "derived image limit exceeded");
                auto derived = cook_image(source.images[binding.image], role, settings,
                                          assets::package_byte_limit - cooked.texture_bytes, stop);
                for (const auto &mip : derived.mips)
                    cooked.texture_bytes += mip.pixels.size();
                cooked.scene.images.push_back(std::move(derived));
                cooked.image_roles.push_back(role);
                image_keys.push_back({binding.image, role});
            }
            if (texture_keys.size() == 256)
                reject(ImportIssue::capacity, "derived texture limit exceeded");
            cooked.scene.textures.push_back({image, binding.sampler});
            texture_keys.push_back({index, role});
            return static_cast<std::uint32_t>(texture_keys.size() - 1);
        };
        for (const auto &material : source.materials) {
            assets::Material output;
            output.name = material.name;
            output.base_color = material.base_color;
            output.emissive = material.emissive;
            output.emissive_strength = material.emissive_strength;
            output.metallic = material.metallic;
            output.roughness = material.roughness;
            output.normal_scale = material.normal_scale;
            output.occlusion_strength = material.occlusion_strength;
            output.alpha_cutoff = material.alpha_cutoff;
            output.alpha = static_cast<std::uint32_t>(material.alpha);
            output.double_sided = material.double_sided;
            const std::array maps{material.base_color_map, material.metallic_roughness_map,
                                  material.normal_map, material.occlusion_map,
                                  material.emissive_map};
            const std::array roles{material.alpha == AlphaMode::opaque ? ImageRole::base_color
                                                                       : ImageRole::alpha_color,
                                   ImageRole::data, ImageRole::normal, ImageRole::data,
                                   ImageRole::emissive};
            for (std::size_t i = 0; i < maps.size(); ++i)
                output.maps[i] = {
                    maps[i].texture == UINT32_MAX ? UINT32_MAX : texture(maps[i].texture, roles[i]),
                    maps[i].uv_set};
            cooked.scene.materials.push_back(std::move(output));
        }
        for (const auto &sampler : source.samplers)
            cooked.scene.samplers.push_back(
                {static_cast<std::uint32_t>(sampler.min), static_cast<std::uint32_t>(sampler.mag),
                 static_cast<std::uint32_t>(sampler.s), static_cast<std::uint32_t>(sampler.t)});
        for (const auto &light : source.lights)
            cooked.scene.lights.push_back({static_cast<std::uint32_t>(light.type), light.color,
                                           light.intensity, light.range,
                                           light.type == LightType::spot ? light.inner_angle : 0,
                                           light.type == LightType::spot ? light.outer_angle : 0});
        for (const auto &instance : source.light_instances)
            cooked.scene.light_instances.push_back(
                {instance.light, instance.node, instance.transform});
        constexpr std::array<double, 3> ratios{1, .4, .1};
        for (const auto &primitive : source.primitives) {
            if (stop.stop_requested())
                return ImportFailure{ImportIssue::cancelled, "scene cooking cancelled"};
            if (primitive.vertices.empty() || primitive.indices.empty() ||
                primitive.indices.size() % 3 != 0)
                return ImportFailure{ImportIssue::invalid,
                                     "cooking requires indexed triangle primitives"};
            assets::Primitive output;
            output.first_vertex = static_cast<std::uint32_t>(cooked.scene.vertices.size());
            output.material = primitive.material;
            output.bounds = {primitive.bounds.minimum, primitive.bounds.maximum};
            std::array<bool, 2> used_uv{};
            bool normal_map = false;
            if (primitive.material != UINT32_MAX) {
                const auto &material = source.materials[primitive.material];
                const std::array maps{material.base_color_map, material.metallic_roughness_map,
                                      material.normal_map, material.occlusion_map,
                                      material.emissive_map};
                for (const auto &map : maps)
                    if (map.texture != UINT32_MAX)
                        used_uv[map.uv_set] = true;
                normal_map = material.normal_map.texture != UINT32_MAX;
            }
            std::vector<unsigned int> position_remap(primitive.vertices.size());
            meshopt_generatePositionRemap(position_remap.data(),
                                          primitive.vertices.front().position.data(),
                                          primitive.vertices.size(), sizeof(Vertex));
            std::vector<unsigned char> locks(primitive.vertices.size());
            for (std::size_t i = 0; i < primitive.vertices.size(); ++i) {
                const auto &v = primitive.vertices[i],
                           &other = primitive.vertices[position_remap[i]];
                if ((used_uv[0] && v.uv != other.uv) || (used_uv[1] && v.uv1 != other.uv1) ||
                    (normal_map && v.tangent[3] != other.tangent[3]))
                    locks[i] = static_cast<unsigned char>(meshopt_SimplifyVertex_Protect);
            }
            constexpr std::array<float, 3> weights{.5F, .5F, .5F};
            std::vector<unsigned int> combined;
            std::array<std::vector<unsigned int>, 3> levels;
            float error_bound = 0;
            for (std::size_t lod = 0; lod < levels.size(); ++lod) {
                auto &indices = levels[lod];
                float error = 0;
                if (lod == 0)
                    indices = primitive.indices;
                else {
                    indices.resize(primitive.indices.size());
                    const auto target = std::max<std::size_t>(
                        3, static_cast<std::size_t>(static_cast<double>(primitive.indices.size()) *
                                                    ratios[lod]) /
                               3 * 3);
                    const auto count = meshopt_simplifyWithAttributes(
                        indices.data(), primitive.indices.data(), primitive.indices.size(),
                        primitive.vertices.front().position.data(), primitive.vertices.size(),
                        sizeof(Vertex), primitive.vertices.front().normal.data(), sizeof(Vertex),
                        weights.data(), weights.size(), locks.data(), target, .01F,
                        meshopt_SimplifyPermissive, &error);
                    indices.resize(count);
                    if (indices.empty()) {
                        indices = primitive.indices;
                        error = 0;
                    }
                }
                if (!std::isfinite(error) || error < 0 || indices.size() % 3 != 0)
                    return ImportFailure{ImportIssue::invalid,
                                         "mesh simplification returned invalid geometry"};
                std::vector<unsigned int> optimized(indices.size());
                meshopt_optimizeVertexCache(optimized.data(), indices.data(), indices.size(),
                                            primitive.vertices.size());
                indices = std::move(optimized);
                // Independent simplification targets can stop at slightly different
                // error-limited counts. A lower LOD must never grow: retain the
                // previous valid level rather than weakening package validation.
                if (lod != 0 && indices.size() > levels[lod - 1].size()) {
                    indices = levels[lod - 1];
                    error = output.lods[lod - 1].error;
                }
                error_bound = std::max(error_bound, error);
                output.lods.push_back({0, static_cast<std::uint32_t>(indices.size()), error_bound});
                cooked.lod_triangles[lod] += indices.size() / 3;
            }
            // Coarsest-first fetch order keeps laptop LOD vertices near the front
            // without changing LOD0 data or the public LOD ordering.
            for (std::size_t level = levels.size(); level != 0; --level) {
                const auto lod = level - 1;
                const auto &indices = levels[lod];
                if (indices.size() >
                    assets::index_limit - cooked.scene.indices.size() - combined.size())
                    return ImportFailure{ImportIssue::capacity, "cooked index limit exceeded"};
                output.lods[lod].first_index =
                    static_cast<std::uint32_t>(cooked.scene.indices.size() + combined.size());
                combined.insert(combined.end(), indices.begin(), indices.end());
            }
            std::vector<unsigned int> remap(primitive.vertices.size());
            const auto count = meshopt_optimizeVertexFetchRemap(
                remap.data(), combined.data(), combined.size(), primitive.vertices.size());
            if (count > assets::vertex_limit - cooked.scene.vertices.size())
                return ImportFailure{ImportIssue::capacity, "cooked vertex limit exceeded"};
            std::vector<Vertex> vertices(count);
            meshopt_remapVertexBuffer(vertices.data(), primitive.vertices.data(),
                                      primitive.vertices.size(), sizeof(Vertex), remap.data());
            std::vector<unsigned int> indices(combined.size());
            meshopt_remapIndexBuffer(indices.data(), combined.data(), combined.size(),
                                     remap.data());
            output.vertex_count = static_cast<std::uint32_t>(vertices.size());
            for (const auto &v : vertices)
                cooked.scene.vertices.push_back({v.position, v.normal, v.uv, v.uv1, v.tangent});
            cooked.scene.indices.insert(cooked.scene.indices.end(), indices.begin(), indices.end());
            cooked.scene.primitives.push_back(std::move(output));
        }
        for (const auto &instance : source.instances)
            cooked.scene.instances.push_back(
                {instance.primitive, instance.node, instance.transform});
        cooked.scene.bounds = {source.bounds.minimum, source.bounds.maximum};
        return cooked;
    } catch (const Rejected &failure) {
        return failure.failure;
    } catch (const std::bad_alloc &) {
        return ImportFailure{ImportIssue::capacity, "scene cooking allocation failed"};
    }
}
} // namespace ngen::assetc
