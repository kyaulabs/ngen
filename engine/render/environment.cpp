#include "render/environment.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <exception>
#include <limits>
#include <numbers>

namespace ngen::render {
namespace {
using Vector = std::array<double, 3>;
constexpr double pi = std::numbers::pi_v<double>;
static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559);
double dot(const Vector &a, const Vector &b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
Vector scale(Vector a, double value) {
    for (auto &x : a)
        x *= value;
    return a;
}
Vector add(Vector a, const Vector &b) {
    for (std::size_t i = 0; i < 3; ++i)
        a[i] += b[i];
    return a;
}
Vector normalized(Vector value) { return scale(value, 1 / std::sqrt(dot(value, value))); }
Vector cross(const Vector &a, const Vector &b) {
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double radical(std::uint32_t value) {
    value = (value << 16U) | (value >> 16U);
    value = ((value & 0x55555555U) << 1U) | ((value & 0xaaaaaaaaU) >> 1U);
    value = ((value & 0x33333333U) << 2U) | ((value & 0xccccccccU) >> 2U);
    value = ((value & 0x0f0f0f0fU) << 4U) | ((value & 0xf0f0f0f0U) >> 4U);
    value = ((value & 0x00ff00ffU) << 8U) | ((value & 0xff00ff00U) >> 8U);
    return static_cast<double>(value) / 4294967296.0;
}
Vector basis(const Vector &normal, Vector local) {
    const auto tangent =
        normalized(cross(std::abs(normal[2]) < .999 ? Vector{0, 0, 1} : Vector{1, 0, 0}, normal));
    const auto bitangent = cross(normal, tangent);
    return add(add(scale(tangent, local[0]), scale(bitangent, local[1])), scale(normal, local[2]));
}
struct CubeTexel {
    std::uint32_t face, x, y, size;
};
Vector cube(CubeTexel texel) {
    const auto [face, x, y, size] = texel;
    const auto u = 2 * (static_cast<double>(x) + .5) / size - 1,
               v = 2 * (static_cast<double>(y) + .5) / size - 1;
    switch (face) {
    case 0:
        return normalized({1, -v, -u});
    case 1:
        return normalized({-1, -v, u});
    case 2:
        return normalized({u, 1, v});
    case 3:
        return normalized({u, -1, -v});
    case 4:
        return normalized({u, -v, 1});
    default:
        return normalized({-u, -v, -1});
    }
}
Vector radiance(const Sky &sky, const Vector &direction) {
    Vector result{};
    const auto t = std::min(1.0, std::abs(direction[1]));
    const auto &end = direction[1] >= 0 ? sky.zenith : sky.ground;
    for (std::size_t i = 0; i < 3; ++i)
        result[i] = (sky.horizon[i] * (1 - t) + end[i] * t) * sky.intensity;
    return result;
}
Vector importance(const Vector &normal, double roughness, std::array<double, 2> sample) {
    const auto a = roughness * roughness;
    const auto phi = 2 * pi * sample[0];
    const auto y = sample[1];
    const auto cosine = std::sqrt((1 - y) / (1 + (a * a - 1) * y));
    const auto sine = std::sqrt(std::max(0.0, 1 - cosine * cosine));
    return basis(normal, {std::cos(phi) * sine, std::sin(phi) * sine, cosine});
}
Vector diffuse(const Sky &sky, const Vector &normal) {
    Vector result{};
    constexpr std::uint32_t count = 64;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto u = static_cast<double>(i) / count, phi = 2 * pi * radical(i);
        const auto direction = basis(
            normal, {std::sqrt(u) * std::cos(phi), std::sqrt(u) * std::sin(phi), std::sqrt(1 - u)});
        result = add(result, radiance(sky, direction));
    }
    return scale(result, pi / count);
}
Vector prefiltered(const Sky &sky, const Vector &normal, double roughness) {
    Vector result{};
    double weight = 0;
    constexpr std::uint32_t count = 64;
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto h = importance(normal, roughness, {static_cast<double>(i) / count, radical(i)});
        const auto light = add(scale(h, 2 * dot(normal, h)), scale(normal, -1));
        const auto nl = dot(normal, light);
        if (nl > 0) {
            result = add(result, scale(radiance(sky, light), nl));
            weight += nl;
        }
    }
    return weight > 0 ? scale(result, 1 / weight) : Vector{};
}
Vector brdf(std::array<double, 2> sample) {
    const auto [nv, roughness] = sample;
    const Vector normal{0, 0, 1}, view{std::sqrt(1 - nv * nv), 0, nv};
    double a = 0, b = 0;
    constexpr std::uint32_t count = 128;
    const auto k = roughness * roughness * .5;
    const auto visibility = [k](double n) { return n / (n * (1 - k) + k); };
    for (std::uint32_t i = 0; i < count; ++i) {
        const auto h = importance(normal, roughness, {static_cast<double>(i) / count, radical(i)});
        const auto vh = std::clamp(dot(view, h), 0.0, 1.0);
        const auto light = add(scale(h, 2 * vh), scale(view, -1));
        const auto nl = std::max(0.0, light[2]);
        if (nl > 0) {
            const auto g = visibility(nv) * visibility(nl) * vh / std::max(1e-12, h[2] * nv);
            const auto fc = std::pow(1 - vh, 5);
            a += (1 - fc) * g;
            b += fc * g;
        }
    }
    return {a / count, b / count, 0};
}
std::uint16_t half(float value) {
    const auto bits = std::bit_cast<std::uint32_t>(value);
    const auto exponent = static_cast<int>((bits >> 23U) & 255U) - 112;
    auto mantissa = bits & 0x7fffffU;
    if (exponent < -10)
        return 0;
    if (exponent <= 0) {
        mantissa |= 0x800000U;
        const auto shift = static_cast<unsigned>(14 - exponent);
        auto result = mantissa >> shift;
        const auto rest = mantissa & ((1U << shift) - 1U), middle = 1U << (shift - 1U);
        if (rest > middle || (rest == middle && (result & 1U) != 0))
            ++result;
        return static_cast<std::uint16_t>(result);
    }
    auto result = (static_cast<std::uint32_t>(exponent) << 10U) | (mantissa >> 13U);
    const auto rest = mantissa & 8191U;
    if (rest > 4096U || (rest == 4096U && (result & 1U) != 0))
        ++result;
    return static_cast<std::uint16_t>(result);
}
void pixel(EnvironmentImage &image, std::size_t index, const Vector &value) {
    std::array<std::uint16_t, 4> output{};
    for (std::size_t i = 0; i < 3; ++i)
        output[i] = half(static_cast<float>(value[i]));
    output[3] = 0x3c00;
    std::memcpy(image.texels.data() + index * 8, output.data(), 8);
}
} // namespace
std::variant<EnvironmentData, Failure> generate_environment(const Sky &sky,
                                                            const std::stop_token &stop) {
    if (stop.stop_requested())
        return Failure{"environment generation cancelled"};
    const auto valid = [](const auto &values) {
        return std::all_of(values.begin(), values.end(), [](float value) {
            return std::isfinite(value) && value >= 0 && value <= 16;
        });
    };
    if (!valid(sky.horizon) || !valid(sky.zenith) || !valid(sky.ground) ||
        !std::isfinite(sky.intensity) || sky.intensity < 0 || sky.intensity > 64)
        return Failure{"sky values exceed the finite environment profile"};
    try {
        EnvironmentData result;
        const auto usage = static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_DST_BIT) |
                           VK_IMAGE_USAGE_SAMPLED_BIT;
        result.images[0].description = {{8, 8}, VK_FORMAT_R16G16B16A16_SFLOAT, usage, 1, true};
        result.images[0].texels.resize(3072);
        result.images[1].description = {{16, 16}, VK_FORMAT_R16G16B16A16_SFLOAT, usage, 5, true};
        result.images[1].texels.resize(16368);
        result.images[2].description = {{64, 64}, VK_FORMAT_R16G16B16A16_SFLOAT, usage};
        result.images[2].texels.resize(32768);
        for (std::size_t kind = 0; kind < 2; ++kind) {
            auto &image = result.images[kind];
            std::size_t index = 0;
            auto size = image.description.extent.width;
            for (std::uint32_t level = 0; level < image.description.mip_levels; ++level) {
                for (std::uint32_t face = 0; face < 6; ++face)
                    for (std::uint32_t y = 0; y < size; ++y)
                        for (std::uint32_t x = 0; x < size; ++x) {
                            if (stop.stop_requested())
                                return Failure{"environment generation cancelled"};
                            const auto normal = cube({.face = face, .x = x, .y = y, .size = size});
                            pixel(image, index++,
                                  kind == 0
                                      ? diffuse(sky, normal)
                                      : prefiltered(sky, normal, static_cast<double>(level) / 4));
                        }
                size = std::max(1U, size / 2);
            }
        }
        for (std::uint32_t y = 0; y < 64; ++y)
            for (std::uint32_t x = 0; x < 64; ++x) {
                if (stop.stop_requested())
                    return Failure{"environment generation cancelled"};
                pixel(result.images[2], static_cast<std::size_t>(y) * 64 + x,
                      brdf({(x + .5) / 64, (y + .5) / 64}));
            }
        return result;
    } catch (const std::exception &error) {
        return Failure{error.what()};
    }
}
} // namespace ngen::render
