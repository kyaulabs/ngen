#include "assetc/tangents.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <meshoptimizer.h>
#include <mikktspace.h>
#include <numeric>
#include <type_traits>

namespace ngen::assetc {
namespace {
static_assert(sizeof(Vertex) == 14 * sizeof(float));
static_assert(std::is_trivially_copyable_v<Vertex>);
static_assert(std::is_same_v<std::uint32_t, unsigned int>);
struct Work {
    const Primitive &source;
    std::vector<Vertex> &corners;
    std::uint32_t uv_set;
};
Work &work(const SMikkTSpaceContext *context) noexcept {
    return *static_cast<Work *>(context->m_pUserData);
}
std::size_t corner_index(int face, int vertex) noexcept {
    return static_cast<std::size_t>(face) * 3 + static_cast<std::size_t>(vertex);
}
const Vertex &source_vertex(const SMikkTSpaceContext *context, int face, int vertex) noexcept {
    const auto &source = work(context).source;
    return source.vertices[source.indices[corner_index(face, vertex)]];
}
int faces(const SMikkTSpaceContext *context) noexcept {
    return static_cast<int>(work(context).source.indices.size() / 3);
}
int vertices(const SMikkTSpaceContext *, int) noexcept { return 3; }
void position(const SMikkTSpaceContext *context, float *output, int face, int vertex) noexcept {
    const auto &value = source_vertex(context, face, vertex).position;
    std::copy(value.begin(), value.end(), output);
}
void normal(const SMikkTSpaceContext *context, float *output, int face, int vertex) noexcept {
    const auto &value = source_vertex(context, face, vertex).normal;
    std::copy(value.begin(), value.end(), output);
}
const std::array<float, 2> &uv(const Vertex &vertex, std::uint32_t set) noexcept {
    return set == 0 ? vertex.uv : vertex.uv1;
}
void texcoord(const SMikkTSpaceContext *context, float *output, int face, int vertex) noexcept {
    const auto &value = uv(source_vertex(context, face, vertex), work(context).uv_set);
    std::copy(value.begin(), value.end(), output);
}
void tangent(const SMikkTSpaceContext *context, const float *value, float sign, int face,
             int vertex) noexcept {
    work(context).corners[corner_index(face, vertex)].tangent = {value[0], value[1], value[2],
                                                                 sign};
}
// MikkTSpace weights with acos(dot(normalized edges)). At a narrow corner the
// float dot can round to 1, giving zero weight and a zero output even for a
// nonsingular face. Recover only that case, from the face's actual UV derivative
// in double precision. Never invent a frame for singular UVs or geometry.
bool repair_rounded_angle(Work &state, std::size_t corner) {
    auto &target = state.corners[corner];
    if (target.tangent[0] != 0 || target.tangent[1] != 0 || target.tangent[2] != 0 ||
        std::abs(target.tangent[3]) != 1)
        return false;
    const auto face = corner / 3 * 3;
    const auto slot = corner % 3;
    const auto &previous = state.corners[face + (slot + 2) % 3];
    const auto &next = state.corners[face + (slot + 1) % 3];
    std::array<double, 3> n{}, e1{}, e2{};
    for (std::size_t axis = 0; axis < 3; ++axis) {
        n[axis] = target.normal[axis];
        e1[axis] = static_cast<double>(previous.position[axis]) - target.position[axis];
        e2[axis] = static_cast<double>(next.position[axis]) - target.position[axis];
    }
    const auto dot = [](const auto &a, const auto &b) {
        return std::inner_product(a.begin(), a.end(), b.begin(), 0.0);
    };
    const double n2 = dot(n, n);
    if (!(n2 > 0))
        return false;
    const auto project = [&](auto &value) {
        const double coefficient = dot(value, n) / n2;
        for (std::size_t axis = 0; axis < 3; ++axis)
            value[axis] -= coefficient * n[axis];
    };
    project(e1);
    project(e2);
    const std::array<double, 3> cross{e1[1] * e2[2] - e1[2] * e2[1], e1[2] * e2[0] - e1[0] * e2[2],
                                      e1[0] * e2[1] - e1[1] * e2[0]};
    const double cross2 = dot(cross, cross);
    if (!(dot(e1, e2) > 0 && cross2 > 0) ||
        cross2 > 16 * std::numeric_limits<float>::epsilon() * dot(e1, e1) * dot(e2, e2))
        return false;
    const auto &a = state.corners[face];
    const auto &b = state.corners[face + 1];
    const auto &c = state.corners[face + 2];
    const auto &ua = uv(a, state.uv_set), &ub = uv(b, state.uv_set), &uc = uv(c, state.uv_set);
    const float u1 = ub[0] - ua[0], v1 = ub[1] - ua[1], u2 = uc[0] - ua[0], v2 = uc[1] - ua[1];
    const float float_area = u1 * v2 - v1 * u2;
    const double area = static_cast<double>(u1) * v2 - static_cast<double>(v1) * u2;
    if (!std::isnormal(float_area) || area == 0 || (area > 0) != (float_area > 0))
        return false;
    std::array<double, 3> direction{};
    const double orientation = area > 0 ? 1 : -1;
    for (std::size_t axis = 0; axis < 3; ++axis)
        direction[axis] =
            orientation * ((static_cast<double>(b.position[axis]) - a.position[axis]) * v2 -
                           (static_cast<double>(c.position[axis]) - a.position[axis]) * v1);
    project(direction);
    const double length = std::sqrt(dot(direction, direction));
    if (!(length > 0) || !std::isfinite(length))
        return false;
    for (std::size_t axis = 0; axis < 3; ++axis)
        target.tangent[axis] = static_cast<float>(direction[axis] / length);
    return valid_frame(target);
}
// For UV-degenerate faces the reference can provide its inherited/default
// axis, which is not necessarily perpendicular to the supplied normal. Keep
// that axis and handedness, applying only the usual normal-plane projection.
// A parallel/zero reference axis still fails; no arbitrary new axis is invented.
bool project_degenerate_reference(Work &state, std::size_t corner) {
    const auto face = corner / 3 * 3;
    const auto &a = uv(state.corners[face], state.uv_set);
    const auto &b = uv(state.corners[face + 1], state.uv_set);
    const auto &c = uv(state.corners[face + 2], state.uv_set);
    const float area = (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
    if (std::abs(area) > std::numeric_limits<float>::min())
        return false;
    auto &vertex = state.corners[corner];
    if (!std::all_of(vertex.tangent.begin(), vertex.tangent.end(),
                     [](float value) { return std::isfinite(value); }) ||
        std::abs(vertex.tangent[3]) != 1)
        return false;
    const std::array<double, 3> n{vertex.normal[0], vertex.normal[1], vertex.normal[2]};
    std::array<double, 3> t{vertex.tangent[0], vertex.tangent[1], vertex.tangent[2]};
    const double nl = std::inner_product(n.begin(), n.end(), n.begin(), 0.0);
    if (!(nl > 0))
        return false;
    const double coefficient = std::inner_product(t.begin(), t.end(), n.begin(), 0.0) / nl;
    for (std::size_t axis = 0; axis < 3; ++axis)
        t[axis] -= coefficient * n[axis];
    const double length = std::sqrt(std::inner_product(t.begin(), t.end(), t.begin(), 0.0));
    if (!(length > 0) || !std::isfinite(length))
        return false;
    for (std::size_t axis = 0; axis < 3; ++axis)
        vertex.tangent[axis] = static_cast<float>(t[axis] / length);
    return valid_frame(vertex);
}
} // namespace
bool valid_frame(const Vertex &vertex) {
    const auto &n = vertex.normal;
    const auto &t = vertex.tangent;
    const auto nl = std::inner_product(n.begin(), n.end(), n.begin(), 0.0F);
    const auto tl = std::inner_product(t.begin(), t.begin() + 3, t.begin(), 0.0F);
    const auto dot = std::inner_product(n.begin(), n.end(), t.begin(), 0.0F);
    return std::abs(nl - 1) <= 0.002F && std::abs(tl - 1) <= 0.002F && std::abs(dot) <= 0.002F &&
           std::abs(t[3]) == 1;
}
std::variant<TangentStatistics, ImportFailure> regenerate_tangents(Primitive &primitive,
                                                                   TangentSettings settings) {
    if (primitive.indices.empty() || primitive.indices.size() % 3 != 0 ||
        primitive.indices.size() > index_limit || settings.uv_set > 1)
        return ImportFailure{ImportIssue::invalid, "invalid tangent-generation input"};
    // The reference algorithm works in float32, including spatial hashing and
    // UV derivatives. Admit a declared local-asset numeric domain before entry.
    Bounds referenced;
    bool first_reference = true;
    for (auto index : primitive.indices) {
        if (index >= primitive.vertices.size())
            return ImportFailure{ImportIssue::invalid, "tangent source index exceeds vertex count"};
        const auto &position = primitive.vertices[index].position;
        if (first_reference) {
            referenced = {position, position};
            first_reference = false;
        }
        for (std::size_t axis = 0; axis < 3; ++axis) {
            referenced.minimum[axis] = std::min(referenced.minimum[axis], position[axis]);
            referenced.maximum[axis] = std::max(referenced.maximum[axis], position[axis]);
        }
    }
    float extent = 0;
    for (std::size_t axis = 0; axis < 3; ++axis)
        extent = std::max(extent, referenced.maximum[axis] - referenced.minimum[axis]);
    if (!std::isfinite(extent) || extent < 1e-12F || extent > 1e6F)
        return ImportFailure{ImportIssue::unsupported,
                             "geometry is outside the tangent cooker numeric domain"};
    for (const auto &vertex : primitive.vertices) {
        for (float value : vertex.position)
            if (!std::isfinite(value) || std::abs(value) > 1e6F)
                return ImportFailure{ImportIssue::unsupported,
                                     "positions exceed the tangent cooker numeric domain"};
        for (float value : uv(vertex, settings.uv_set))
            if (!std::isfinite(value) || std::abs(value) > 1e6F)
                return ImportFailure{ImportIssue::unsupported,
                                     "UV values exceed the tangent cooker numeric domain"};
    }
    TangentStatistics statistics;
    for (std::size_t face = 0; face < primitive.indices.size(); face += 3) {
        const auto &a = primitive.vertices[primitive.indices[face]];
        const auto &b = primitive.vertices[primitive.indices[face + 1]];
        const auto &c = primitive.vertices[primitive.indices[face + 2]];
        const auto &ua = uv(a, settings.uv_set);
        const auto &ub = uv(b, settings.uv_set);
        const auto &uc = uv(c, settings.uv_set);
        const float u1 = ub[0] - ua[0], v1 = ub[1] - ua[1];
        const float u2 = uc[0] - ua[0], v2 = uc[1] - ua[1];
        const float area = u1 * v2 - v1 * u2;
        if (std::abs(area) <= std::numeric_limits<float>::min()) {
            ++statistics.degenerate_uv_triangles;
            continue;
        }
        for (std::size_t axis = 0; axis < 3; ++axis) {
            const double p1 = static_cast<double>(b.position[axis]) - a.position[axis];
            const double p2 = static_cast<double>(c.position[axis]) - a.position[axis];
            if (std::abs((p1 * v2 - p2 * v1) / area) > 1e18 ||
                std::abs((p2 * u1 - p1 * u2) / area) > 1e18)
                return ImportFailure{ImportIssue::unsupported,
                                     "UV derivatives exceed the tangent cooker numeric domain"};
        }
    }
    std::vector<Vertex> corners(primitive.indices.size());
    for (std::size_t i = 0; i < corners.size(); ++i) {
        corners[i] = primitive.vertices[primitive.indices[i]];
        corners[i].tangent.fill(std::numeric_limits<float>::quiet_NaN());
    }
    Work state{primitive, corners, settings.uv_set};
    SMikkTSpaceInterface interface{faces, vertices, position, normal, texcoord, tangent, nullptr};
    SMikkTSpaceContext context{&interface, &state};
    if (genTangSpaceDefault(&context) == 0)
        return ImportFailure{ImportIssue::invalid, "MikkTSpace tangent generation failed"};
    for (std::size_t i = 0; i < corners.size(); ++i) {
        if (valid_frame(corners[i]))
            continue;
        if (statistics.derivative_fallback_corners + statistics.degenerate_projection_corners ==
            settings.fallback_corner_allowance)
            return ImportFailure{ImportIssue::capacity, "tangent repair limit exceeded"};
        if (repair_rounded_angle(state, i)) {
            ++statistics.derivative_fallback_corners;
            continue;
        }
        if (project_degenerate_reference(state, i)) {
            ++statistics.degenerate_projection_corners;
            continue;
        }
        return ImportFailure{
            ImportIssue::invalid,
            "MikkTSpace cannot produce a valid frame at corner " + std::to_string(i) +
                " tangent=" + std::to_string(corners[i].tangent[0]) + "," +
                std::to_string(corners[i].tangent[1]) + "," +
                std::to_string(corners[i].tangent[2]) + "," +
                std::to_string(corners[i].tangent[3]) +
                " normal=" + std::to_string(corners[i].normal[0]) + "," +
                std::to_string(corners[i].normal[1]) + "," + std::to_string(corners[i].normal[2])};
    }
    // Results are PER CORNER. Never overwrite the old indexed vertices: shared
    // corners may need different signs/tangents across mirrored UV discontinuities.
    std::vector<unsigned int> remap(corners.size());
    const auto count = meshopt_generateVertexRemap(remap.data(), nullptr, corners.size(),
                                                   corners.data(), corners.size(), sizeof(Vertex));
    if (count > settings.vertex_allowance)
        return ImportFailure{ImportIssue::capacity,
                             "rebuilt tangent vertices exceed the scene budget"};
    std::vector<Vertex> output(count);
    std::vector<std::uint32_t> indices(corners.size());
    meshopt_remapVertexBuffer(output.data(), corners.data(), corners.size(), sizeof(Vertex),
                              remap.data());
    meshopt_remapIndexBuffer(indices.data(), nullptr, corners.size(), remap.data());
    primitive.vertices = std::move(output);
    primitive.indices = std::move(indices);
    statistics.rebuilt_primitives = 1;
    statistics.rebuilt_corners = corners.size();
    return statistics;
}
} // namespace ngen::assetc
