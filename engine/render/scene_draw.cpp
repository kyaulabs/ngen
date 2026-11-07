#include "render/scene_draw.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace ngen::render {
namespace {
constexpr std::size_t draw_limit = 256;
using Vector = std::array<double, 3>;
Vector cross(const Vector &a, const Vector &b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double dot(const Vector &a, const Vector &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
std::optional<double> normal(const std::array<float, 16> &model, shader::Matrix &output) {
    const Vector a{model[0], model[1], model[2]}, b{model[4], model[5], model[6]},
        c{model[8], model[9], model[10]};
    const auto bc = cross(b, c);
    const auto determinant = dot(a, bc);
    if (!std::isfinite(determinant) || determinant == 0)
        return {};
    const std::array columns{bc, cross(c, a), cross(a, b)};
    output = {};
    output.columns[15] = 1;
    for (std::size_t column = 0; column < 3; ++column)
        for (std::size_t row = 0; row < 3; ++row) {
            const auto value = columns[column][row] / determinant;
            if (!std::isfinite(value) || std::abs(value) > std::numeric_limits<float>::max())
                return {};
            output.columns[column * 4 + row] = static_cast<float>(value);
        }
    return determinant;
}
MeshSampler sampler(const assets::Sampler *source) {
    MeshSampler output{};
    if (!source)
        return output;
    output.mag = source->mag == 9728 ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    switch (source->min) {
    case 9728:
        output.min = VK_FILTER_NEAREST;
        output.mipmaps = false;
        break;
    case 9729:
        output.min = VK_FILTER_LINEAR;
        output.mipmaps = false;
        break;
    case 9984:
        output.min = VK_FILTER_NEAREST;
        output.mip = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    case 9985:
        output.min = VK_FILTER_LINEAR;
        output.mip = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        break;
    case 9986:
        output.min = VK_FILTER_NEAREST;
        output.mip = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        break;
    default:
        break;
    }
    const auto address = [](std::uint32_t value) {
        return value == 33071 ? VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE
                              : (value == 33648 ? VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT
                                                : VK_SAMPLER_ADDRESS_MODE_REPEAT);
    };
    output.u = address(source->s);
    output.v = address(source->t);
    return output;
}
// Fixed forward tier: retain all CPU definitions, choose the 16 point-light
// instances with greatest estimated irradiance at the admitted scene center.
shader::LightConstants lights(const assets::Scene &scene) {
    struct Selection {
        double score{-1};
        std::uint32_t instance{};
    };
    std::array<Selection, shader::local_light_capacity> selected{};
    Vector center{};
    double radius2 = 0;
    for (std::size_t i = 0; i < 3; ++i) {
        center[i] = (static_cast<double>(scene.bounds.minimum[i]) + scene.bounds.maximum[i]) * .5;
        const auto half =
            (static_cast<double>(scene.bounds.maximum[i]) - scene.bounds.minimum[i]) * .5;
        radius2 += half * half;
    }
    for (std::size_t i = 0; i < scene.light_instances.size(); ++i) {
        const auto &instance = scene.light_instances[i];
        const auto &light = scene.lights[instance.light];
        Vector delta{};
        for (std::size_t axis = 0; axis < 3; ++axis)
            delta[axis] = instance.transform[12 + axis] - center[axis];
        const auto distance2 = dot(delta, delta);
        if (light.range > 0 &&
            std::sqrt(distance2) > static_cast<double>(light.range) + std::sqrt(radius2))
            continue;
        const auto score = static_cast<double>(light.intensity) / std::max(1.0, distance2);
        if (score <= 0)
            continue;
        for (std::size_t slot = 0; slot < selected.size(); ++slot)
            if (score > selected[slot].score) {
                for (std::size_t tail = selected.size() - 1; tail > slot; --tail)
                    selected[tail] = selected[tail - 1];
                selected[slot] = {score, static_cast<std::uint32_t>(i)};
                break;
            }
    }
    shader::LightConstants output{};
    for (const auto &choice : selected) {
        if (choice.score < 0)
            break;
        const auto &instance = scene.light_instances[choice.instance];
        const auto &light = scene.lights[instance.light];
        auto &native = output.lights[output.count++];
        native.positionRange = {instance.transform[12], instance.transform[13],
                                instance.transform[14], light.range};
        native.colorIntensity = {light.color[0], light.color[1], light.color[2], light.intensity};
    }
    return output;
}
} // namespace
struct SceneRenderer::Impl {
    struct Frame {
        ScenePin version;
        Submission submission;
    };
    Context *owner{};
    std::array<Frame, 2> frames;
    std::array<MeshDraw, draw_limit> draws, ordered;
    std::array<std::size_t, draw_limit> order{};
    std::array<double, draw_limit> depths{};
};
SceneRenderer::SceneRenderer() : impl_(std::make_unique<Impl>()) {}
SceneRenderer::~SceneRenderer() = default;
std::variant<std::size_t, Failure> SceneRenderer::collect(Context &context) {
    if (impl_->owner && impl_->owner != &context)
        return Failure{"scene renderer belongs to another context"};
    std::size_t pending = 0;
    for (auto &frame : impl_->frames)
        if (frame.version) {
            const auto complete = context.submission_complete(frame.submission);
            if (const auto *failure = std::get_if<Failure>(&complete))
                return *failure;
            if (std::get<bool>(complete))
                frame = {};
            else
                ++pending;
        }
    return pending;
}
std::variant<Submission, Failure> SceneRenderer::draw(Context &context, ScenePin version,
                                                      const SceneView &view, SceneTargets targets,
                                                      TimelineWait wait) {
    if (!version)
        return Failure{"scene draw has no immutable version"};
    const auto finite = [](const auto &values) {
        return std::all_of(values.begin(), values.end(),
                           [](float value) { return std::isfinite(value); });
    };
    if (!finite(view.view_projection.columns) || !finite(view.light_view_projection.columns) ||
        !finite(view.camera_position) || !finite(view.key_direction_intensity) ||
        !finite(view.key_color))
        return Failure{"scene view must be finite"};
    const auto collected = collect(context);
    if (const auto *failure = std::get_if<Failure>(&collected))
        return *failure;
    auto slot = std::find_if(impl_->frames.begin(), impl_->frames.end(),
                             [](const auto &frame) { return !frame.version; });
    if (slot == impl_->frames.end())
        return Failure{"scene frame pins are busy"};
    const auto package = version->package();
    if (!package || package->instances.empty() || package->instances.size() > draw_limit ||
        view.lod > 2)
        return Failure{"scene draw exceeds the fixed instance/LOD profile"};
    for (const auto &light : package->lights)
        if (light.type != 1)
            return Failure{"scene forward tier currently requires point-light definitions"};
    const auto point_lights = lights(*package);
    const assets::Material default_material{};
    for (std::size_t i = 0; i < package->instances.size(); ++i) {
        const auto &instance = package->instances[i];
        const auto &primitive = package->primitives[instance.primitive];
        const auto &material = primitive.material == UINT32_MAX
                                   ? default_material
                                   : package->materials[primitive.material];
        if (primitive.lods.empty() || primitive.first_vertex > INT32_MAX)
            return Failure{"scene primitive has no representable indexed draw"};
        auto &draw = impl_->draws[i];
        draw = {};
        const auto lod = std::min(static_cast<std::size_t>(view.lod), primitive.lods.size() - 1);
        draw.first_index = primitive.lods[lod].first_index;
        draw.index_count = primitive.lods[lod].index_count;
        draw.vertex_offset = static_cast<std::int32_t>(primitive.first_vertex);
        draw.frame.model.columns = instance.transform;
        const auto determinant = normal(instance.transform, draw.frame.normalMatrix);
        if (!determinant)
            return Failure{"scene model normal transform is not representable"};
        draw.frame.viewProjection = view.view_projection;
        draw.frame.lightViewProjection = view.light_view_projection;
        draw.frame.cameraPosition = view.camera_position;
        draw.frame.keyDirectionIntensity = view.key_direction_intensity;
        draw.frame.keyColor = view.key_color;
        draw.facing = material.double_sided ? MeshFacing::both
                                            : (*determinant < 0 ? MeshFacing::clockwise
                                                                : MeshFacing::counter_clockwise);
        draw.material.baseColor = material.base_color;
        for (std::size_t channel = 0; channel < 3; ++channel)
            draw.material.emissiveAlphaCutoff[channel] =
                material.emissive[channel] * material.emissive_strength;
        draw.material.emissiveAlphaCutoff[3] = material.alpha_cutoff;
        draw.material.metallicRoughnessNormalOcclusion = {material.metallic, material.roughness,
                                                          material.normal_scale,
                                                          material.occlusion_strength};
        draw.material.flags[0] = material.alpha;
        draw.material.flags[2] =
            (material.maps[2].texture == UINT32_MAX ? 1U : 0U) | (material.double_sided ? 2U : 0U);
        for (std::size_t map = 0; map < 5; ++map) {
            const auto &reference = material.maps[map];
            draw.maps[map] = &targets.default_texture;
            if (reference.texture != UINT32_MAX) {
                const auto &texture = package->textures[reference.texture];
                draw.maps[map] = &version->images()[texture.image];
                draw.samplers[map] = sampler(
                    texture.sampler == UINT32_MAX ? nullptr : &package->samplers[texture.sampler]);
                if (reference.uv_set == 1)
                    draw.material.flags[1] |= 1U << map;
            }
        }
        std::array<double, 4> center{0, 0, 0, 1};
        for (std::size_t row = 0; row < 3; ++row) {
            center[row] = instance.transform[12 + row];
            for (std::size_t column = 0; column < 3; ++column)
                center[row] += static_cast<double>(instance.transform[column * 4 + row]) *
                               (static_cast<double>(primitive.bounds.minimum[column]) +
                                primitive.bounds.maximum[column]) *
                               .5;
        }
        double z = 0, w = 0;
        for (std::size_t column = 0; column < 4; ++column) {
            z += view.view_projection.columns[column * 4 + 2] * center[column];
            w += view.view_projection.columns[column * 4 + 3] * center[column];
        }
        impl_->depths[i] = w == 0 ? std::numeric_limits<double>::max() : z / w;
        impl_->order[i] = i;
    }
    const auto count = package->instances.size();
    std::sort(impl_->order.begin(), impl_->order.begin() + static_cast<std::ptrdiff_t>(count),
              [&](std::size_t a, std::size_t b) {
                  const bool transparent_a = impl_->draws[a].material.flags[0] == 2,
                             transparent_b = impl_->draws[b].material.flags[0] == 2;
                  if (transparent_a != transparent_b)
                      return !transparent_a;
                  if (transparent_a && impl_->depths[a] != impl_->depths[b])
                      return impl_->depths[a] > impl_->depths[b];
                  return a < b;
              });
    for (std::size_t i = 0; i < count; ++i)
        impl_->ordered[i] = impl_->draws[impl_->order[i]];
    const MeshPass pass{version->vertices(),
                        version->indices(),
                        targets.color,
                        targets.depth,
                        targets.shadow,
                        targets.irradiance,
                        targets.prefiltered,
                        targets.brdf,
                        point_lights,
                        std::span(impl_->ordered).first(count),
                        wait};
    auto result = context.draw_mesh(pass);
    if (const auto *submission = std::get_if<Submission>(&result)) {
        impl_->owner = &context;
        slot->version = std::move(version);
        slot->submission = *submission;
    }
    return result;
}
} // namespace ngen::render
