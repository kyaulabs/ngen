#include "assets/package.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <exception>
#include <fstream>
#include <numbers>
#include <numeric>
#include <string_view>
#include <zlib.h>

namespace ngen::assets {
namespace {
struct Rejected : std::exception {
    Failure failure;
    explicit Rejected(Failure value) : failure(std::move(value)) {}
    const char *what() const noexcept override { return failure.message.c_str(); }
};
[[noreturn]] void reject(Issue issue, const char *message) { throw Rejected{{issue, message}}; }
void cancelled(const std::stop_token &stop) {
    if (stop.stop_requested())
        reject(Issue::cancelled, "package load cancelled");
}
struct Reader {
    std::span<const std::byte> bytes;
    std::uint32_t u32(std::size_t offset) const {
        if (offset > bytes.size() || bytes.size() - offset < 4)
            reject(Issue::invalid, "truncated package field");
        std::uint32_t value = 0;
        for (unsigned i = 0; i < 4; ++i)
            value |= std::to_integer<std::uint32_t>(bytes[offset + i]) << (i * 8U);
        return value;
    }
    std::uint64_t u64(std::size_t offset) const {
        return u32(offset) | (std::uint64_t{u32(offset + 4)} << 32U);
    }
    float f32(std::size_t offset) const {
        const float value = std::bit_cast<float>(u32(offset));
        if (!std::isfinite(value))
            reject(Issue::invalid, "package numeric field is not finite");
        return value;
    }
    template <std::size_t N> std::array<float, N> floats(std::size_t offset) const {
        std::array<float, N> out{};
        for (std::size_t i = 0; i < N; ++i)
            out[i] = f32(offset + i * 4);
        return out;
    }
};
struct Section {
    std::size_t offset{}, bytes{};
    std::uint32_t count{};
};
std::array<Section, 13> directory(Reader reader) {
    if (reader.bytes.size() > package_byte_limit)
        reject(Issue::capacity, "package byte limit exceeded");
    if (reader.bytes.size() < 480)
        reject(Issue::invalid, "truncated package header/directory");
    constexpr std::array<char, 8> magic{'N', 'G', 'A', 'S', 'S', 'E', 'T', '\0'};
    for (std::size_t i = 0; i < magic.size(); ++i)
        if (reader.bytes[i] != static_cast<std::byte>(magic[i]))
            reject(Issue::invalid, "invalid package magic");
    if (reader.u32(8) != 1)
        reject(Issue::unsupported, "unsupported package schema version");
    if (reader.u32(12) != 64 || reader.u32(28) != 13 || reader.u64(16) != reader.bytes.size())
        reject(Issue::invalid, "package size or section directory mismatch");
    for (std::size_t i = 32; i < 64; ++i)
        if (reader.bytes[i] != std::byte{0})
            reject(Issue::unsupported, "unsupported package header flags");
    const auto crc = static_cast<std::uint32_t>(crc32_z(
        0, reinterpret_cast<const Bytef *>(reader.bytes.data() + 64), reader.bytes.size() - 64));
    if (crc != reader.u32(24))
        reject(Issue::invalid, "package checksum mismatch");
    constexpr std::array<std::uint32_t, 13> strides{56, 4, 80, 128, 8, 16, 32, 1, 72, 32, 72, 1, 1};
    constexpr std::array<std::size_t, 13> limits{vertex_limit,
                                                 index_limit,
                                                 primitive_limit,
                                                 256,
                                                 256,
                                                 64,
                                                 256,
                                                 package_byte_limit,
                                                 4096,
                                                 4096,
                                                 4096,
                                                 std::size_t{1024} * 1024,
                                                 std::size_t{64} * 1024};
    std::array<Section, 13> result{};
    std::size_t cursor = 480;
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto record = 64 + i * 32;
        if (reader.u32(record) != i + 1 || reader.u32(record + 4) != strides[i] ||
            reader.u32(record + 12) != 0)
            reject(Issue::unsupported, "unsupported package section type or stride");
        const auto count = reader.u32(record + 8);
        if (count > limits[i])
            reject(Issue::capacity, "package section count limit exceeded");
        const auto offset = reader.u64(record + 16), length = reader.u64(record + 24);
        if (offset > reader.bytes.size() || length > reader.bytes.size() - offset ||
            length != std::uint64_t{count} * strides[i] ||
            offset != ((cursor + 7) & ~std::size_t{7}))
            reject(Issue::invalid, "package section range/overlap mismatch");
        for (auto pad = cursor; pad < offset; ++pad)
            if (reader.bytes[pad] != std::byte{0})
                reject(Issue::invalid, "nonzero package alignment padding");
        result[i] = {static_cast<std::size_t>(offset), static_cast<std::size_t>(length), count};
        cursor = static_cast<std::size_t>(offset + length);
    }
    if (cursor != reader.bytes.size())
        reject(Issue::invalid, "unclaimed trailing package bytes");
    return result;
}
struct ImageRecord {
    std::size_t offset{}, bytes{};
    std::uint32_t width{}, height{}, levels{};
    ImageFormat format{ImageFormat::rgba8_unorm};
};
Image native_texture(std::span<const std::byte> data, const ImageRecord &record,
                     std::size_t allowance, const std::stop_token &stop) {
    Reader reader{data};
    constexpr std::array<unsigned, 12> identifier{0xab, 'K',  'T', 'X', ' ', '2',
                                                  '0',  0xbb, 13,  10,  26,  10};
    if (data.size() < 80)
        reject(Issue::invalid, "truncated native KTX2 header");
    for (std::size_t i = 0; i < identifier.size(); ++i)
        if (std::to_integer<unsigned>(data[i]) != identifier[i])
            reject(Issue::invalid, "invalid KTX2 identifier");
    const auto format = reader.u32(12), width = reader.u32(20), height = reader.u32(24),
               levels = reader.u32(40);
    const auto expected_format = record.format == ImageFormat::rgba8_srgb ? 43U : 37U;
    if (format != expected_format || reader.u32(16) != 1 || reader.u32(28) != 0 ||
        reader.u32(32) != 0 || reader.u32(36) != 1 || reader.u32(44) != 0 || reader.u64(64) != 0 ||
        reader.u64(72) != 0)
        reject(Issue::unsupported,
               "only native 2D RGBA8 KTX2 without supercompression is admitted");
    if (width == 0 || height == 0 || width != record.width || height != record.height ||
        levels != record.levels ||
        levels != static_cast<std::uint32_t>(std::bit_width(std::max(width, height))))
        reject(Issue::invalid, "KTX2 extent or complete mip chain mismatch");
    if (width > 8192 || height > 8192 || levels > 14)
        reject(Issue::capacity, "KTX2 image extent limit exceeded");
    const std::size_t index_end = 80 + std::size_t{levels} * 24;
    if (index_end > data.size())
        reject(Issue::invalid, "truncated KTX2 level index");
    struct Range {
        std::size_t offset{}, bytes{};
    };
    const auto checked = [&](std::uint64_t offset, std::uint64_t bytes) {
        if (offset < index_end || offset % 4 != 0 || offset > data.size() ||
            bytes > data.size() - offset)
            reject(Issue::invalid, "KTX2 section range exceeds its image");
        return Range{static_cast<std::size_t>(offset), static_cast<std::size_t>(bytes)};
    };
    std::vector<Range> ranges;
    ranges.reserve(levels + 2);
    const auto dfd = checked(reader.u32(48), reader.u32(52));
    ranges.push_back(dfd);
    if (dfd.bytes != 92 || reader.u32(dfd.offset) != 92 || reader.u32(dfd.offset + 4) != 0 ||
        reader.u32(dfd.offset + 8) != ((88U << 16U) | 2U))
        reject(Issue::unsupported, "unsupported KTX2 data-format descriptor");
    const auto byte = [&](std::size_t i) {
        return std::to_integer<unsigned>(data[dfd.offset + i]);
    };
    if (byte(12) != 1 || byte(13) != 1 ||
        byte(14) != (record.format == ImageFormat::rgba8_srgb ? 2U : 1U) || byte(15) != 0 ||
        reader.u32(dfd.offset + 16) != 0 || reader.u32(dfd.offset + 20) != 4 ||
        reader.u32(dfd.offset + 24) != 0)
        reject(Issue::unsupported, "unsupported KTX2 transfer/block/alpha interpretation");
    for (std::size_t sample = 0; sample < 4; ++sample) {
        const auto offset = dfd.offset + 28 + sample * 16;
        const auto bits = reader.u32(offset);
        const auto channel = sample == 3 ? (record.format == ImageFormat::rgba8_srgb ? 31U : 15U)
                                         : static_cast<unsigned>(sample);
        if ((bits & 65535U) != sample * 8 || ((bits >> 16U) & 255U) != 7 ||
            (bits >> 24U) != channel || reader.u32(offset + 4) != 0 ||
            reader.u32(offset + 8) != 0 || reader.u32(offset + 12) != 255)
            reject(Issue::unsupported, "unsupported KTX2 RGBA8 sample layout");
    }
    const auto kvd_offset = reader.u32(56), kvd_bytes = reader.u32(60);
    if ((kvd_offset == 0) != (kvd_bytes == 0) || kvd_bytes > 64 * 1024)
        reject(Issue::invalid, "invalid KTX2 metadata range");
    if (kvd_bytes != 0) {
        const auto kvd = checked(kvd_offset, kvd_bytes);
        ranges.push_back(kvd);
        const auto end = kvd.offset + kvd.bytes;
        std::size_t cursor = kvd.offset;
        std::vector<std::string_view> keys;
        keys.reserve(64);
        while (cursor < end) {
            if (end - cursor < 4)
                reject(Issue::invalid, "truncated KTX2 metadata length");
            const auto count = reader.u32(cursor);
            cursor += 4;
            if (count == 0 || count > end - cursor || keys.size() == 64)
                reject(Issue::invalid, "invalid KTX2 metadata entry");
            const std::string_view entry{reinterpret_cast<const char *>(data.data() + cursor),
                                         count};
            const auto split = entry.find('\0');
            if (split == 0 || split == std::string_view::npos || split > 256)
                reject(Issue::invalid, "invalid KTX2 metadata key");
            const auto key = entry.substr(0, split), value = entry.substr(split + 1);
            if (std::find(keys.begin(), keys.end(), key) != keys.end())
                reject(Issue::invalid, "duplicate KTX2 metadata key");
            keys.push_back(key);
            if (key == "KTXorientation" && value != "rd" && value != std::string_view{"rd\0", 3})
                reject(Issue::unsupported, "KTX2 requires top-left texture coordinates");
            if (key == "KTXswizzle")
                reject(Issue::unsupported, "runtime texture swizzles are not admitted");
            cursor += count;
            const auto aligned = (cursor + 3) & ~std::size_t{3};
            if (aligned > end)
                reject(Issue::invalid, "invalid KTX2 metadata padding");
            while (cursor < aligned)
                if (data[cursor++] != std::byte{0})
                    reject(Issue::invalid, "nonzero KTX2 metadata padding");
        }
    }
    Image image;
    image.format = record.format;
    image.mips.reserve(levels);
    std::vector<Range> pixels;
    pixels.reserve(levels);
    std::size_t total = 0;
    auto w = width, h = height;
    for (std::size_t mip = 0; mip < levels; ++mip) {
        const auto offset = 80 + mip * 24;
        const auto range = checked(reader.u64(offset), reader.u64(offset + 8));
        const auto expected = std::size_t{w} * h * 4;
        if (range.bytes != expected || reader.u64(offset + 16) != expected)
            reject(Issue::invalid, "KTX2 mip byte count mismatch");
        if (expected > allowance - total)
            reject(Issue::capacity, "aggregate texture byte limit exceeded");
        total += expected;
        ranges.push_back(range);
        pixels.push_back(range);
        image.mips.push_back({w, h, {}});
        w = std::max(1U, w / 2);
        h = std::max(1U, h / 2);
    }
    std::sort(ranges.begin(), ranges.end(),
              [](const Range &a, const Range &b) { return a.offset < b.offset; });
    auto cursor = index_end;
    for (const auto &range : ranges) {
        if (range.offset < cursor)
            reject(Issue::invalid, "overlapping KTX2 sections");
        while (cursor < range.offset)
            if (data[cursor++] != std::byte{0})
                reject(Issue::invalid, "unclaimed KTX2 bytes");
        cursor = range.offset + range.bytes;
    }
    if (cursor != data.size())
        reject(Issue::invalid, "trailing KTX2 bytes");
    for (std::size_t i = 0; i < pixels.size(); ++i) {
        cancelled(stop);
        const auto bytes = data.subspan(pixels[i].offset, pixels[i].bytes);
        image.mips[i].pixels.assign(bytes.begin(), bytes.end());
    }
    return image;
}
void load_textures(Reader reader, const std::array<Section, 13> &sections, Scene &scene,
                   const std::stop_token &stop) {
    std::size_t cursor = 0, admitted = 0;
    scene.images.reserve(sections[6].count);
    for (std::size_t i = 0; i < sections[6].count; ++i) {
        cancelled(stop);
        const auto offset = sections[6].offset + i * 32;
        const auto start = reader.u64(offset), bytes = reader.u64(offset + 8);
        const auto format = reader.u32(offset + 28);
        if (format < 1 || format > 2)
            reject(Issue::unsupported, "unsupported package image format");
        if (start != ((cursor + 7) & ~std::size_t{7}) || start > sections[7].bytes ||
            bytes > sections[7].bytes - start || bytes == 0)
            reject(Issue::invalid, "invalid package image range");
        while (cursor < start)
            if (reader.bytes[sections[7].offset + cursor++] != std::byte{0})
                reject(Issue::invalid, "nonzero package image padding");
        ImageRecord record{static_cast<std::size_t>(start), static_cast<std::size_t>(bytes),
                           reader.u32(offset + 16),         reader.u32(offset + 20),
                           reader.u32(offset + 24),         static_cast<ImageFormat>(format)};
        auto image =
            native_texture(reader.bytes.subspan(sections[7].offset + record.offset, record.bytes),
                           record, package_byte_limit - admitted, stop);
        for (const auto &mip : image.mips)
            admitted += mip.pixels.size();
        scene.images.push_back(std::move(image));
        cursor = static_cast<std::size_t>(start + bytes);
    }
    if (cursor != sections[7].bytes)
        reject(Issue::invalid, "unclaimed package image data");
    scene.samplers.reserve(sections[5].count);
    const auto min_valid = [](std::uint32_t value) {
        return value == 0 || value == 9728 || value == 9729 || (value >= 9984 && value <= 9987);
    };
    const auto wrap_valid = [](std::uint32_t value) {
        return value == 10497 || value == 33071 || value == 33648;
    };
    for (std::size_t i = 0; i < sections[5].count; ++i) {
        const auto offset = sections[5].offset + i * 16;
        Sampler sampler{reader.u32(offset), reader.u32(offset + 4), reader.u32(offset + 8),
                        reader.u32(offset + 12)};
        if (!min_valid(sampler.min) ||
            (sampler.mag != 0 && sampler.mag != 9728 && sampler.mag != 9729) ||
            !wrap_valid(sampler.s) || !wrap_valid(sampler.t))
            reject(Issue::invalid, "invalid package sampler");
        scene.samplers.push_back(sampler);
    }
    scene.textures.reserve(sections[4].count);
    for (std::size_t i = 0; i < sections[4].count; ++i) {
        const auto offset = sections[4].offset + i * 8;
        Texture texture{reader.u32(offset), reader.u32(offset + 4)};
        if (texture.image >= scene.images.size() ||
            (texture.sampler != UINT32_MAX && texture.sampler >= scene.samplers.size()))
            reject(Issue::invalid, "invalid package texture reference");
        scene.textures.push_back(texture);
    }
}
void load_materials(Reader reader, const std::array<Section, 13> &sections, Scene &scene) {
    const auto unit = [](float value) { return value >= 0 && value <= 1; };
    scene.materials.reserve(sections[3].count);
    for (std::size_t i = 0; i < sections[3].count; ++i) {
        const auto offset = sections[3].offset + i * 128;
        Material material;
        material.base_color = reader.floats<4>(offset);
        material.emissive = reader.floats<3>(offset + 16);
        material.emissive_strength = reader.f32(offset + 28);
        material.metallic = reader.f32(offset + 32);
        material.roughness = reader.f32(offset + 36);
        material.normal_scale = reader.f32(offset + 40);
        material.occlusion_strength = reader.f32(offset + 44);
        material.alpha_cutoff = reader.f32(offset + 48);
        material.alpha = reader.u32(offset + 52);
        const auto sided = reader.u32(offset + 56);
        material.double_sided = sided != 0;
        if (!std::all_of(material.base_color.begin(), material.base_color.end(), unit) ||
            !std::all_of(material.emissive.begin(), material.emissive.end(), unit) ||
            material.emissive_strength < 0 || !unit(material.metallic) ||
            !unit(material.roughness) || !unit(material.occlusion_strength) ||
            material.alpha_cutoff < 0 || material.alpha > 2 || sided > 1)
            reject(Issue::invalid, "package material factors are outside admitted limits");
        for (float factor : material.emissive)
            if (!std::isfinite(factor * material.emissive_strength))
                reject(Issue::invalid, "package material emission overflows");
        for (std::size_t map = 0; map < material.maps.size(); ++map) {
            auto &reference = material.maps[map];
            reference = {reader.u32(offset + 60 + map * 8), reader.u32(offset + 64 + map * 8)};
            if ((reference.texture != UINT32_MAX && reference.texture >= sections[4].count) ||
                reference.uv_set > 1 || (reference.texture == UINT32_MAX && reference.uv_set != 0))
                reject(Issue::invalid, "package material texture/UV reference is invalid");
            if (reference.texture != UINT32_MAX) {
                const auto expected =
                    (map == 0 || map == 4) ? ImageFormat::rgba8_srgb : ImageFormat::rgba8_unorm;
                if (scene.images[scene.textures[reference.texture].image].format != expected)
                    reject(Issue::invalid, "material texture color-space mismatch");
            }
        }
        const auto name_offset = reader.u32(offset + 100), name_bytes = reader.u32(offset + 104);
        if (name_bytes > 256 || name_offset > sections[11].bytes ||
            name_bytes > sections[11].bytes - name_offset)
            reject(Issue::invalid, "package material name range is invalid");
        const auto name = reader.bytes.subspan(sections[11].offset + name_offset, name_bytes);
        for (auto byte : name)
            if (std::to_integer<unsigned>(byte) < 32 || byte == std::byte{127})
                reject(Issue::invalid, "control byte in package material name");
        material.name.assign(reinterpret_cast<const char *>(name.data()), name.size());
        for (std::size_t reserved = 108; reserved < 128; reserved += 4)
            if (reader.u32(offset + reserved) != 0)
                reject(Issue::unsupported, "unsupported material record fields");
        scene.materials.push_back(std::move(material));
    }
}
void bounds_valid(const Bounds &bounds) {
    for (std::size_t i = 0; i < 3; ++i)
        if (bounds.minimum[i] > bounds.maximum[i])
            reject(Issue::invalid, "inverted primitive bounds");
}
void transform_valid(const std::array<float, 16> &m) {
    if (m[3] != 0 || m[7] != 0 || m[11] != 0 || m[15] != 1)
        reject(Issue::invalid, "package transform must be affine");
    const auto a = [&](std::size_t c, std::size_t r) { return static_cast<double>(m[c * 4 + r]); };
    const double determinant = a(0, 0) * (a(1, 1) * a(2, 2) - a(2, 1) * a(1, 2)) -
                               a(1, 0) * (a(0, 1) * a(2, 2) - a(2, 1) * a(0, 2)) +
                               a(2, 0) * (a(0, 1) * a(1, 2) - a(1, 1) * a(0, 2));
    if (determinant == 0)
        reject(Issue::unsupported, "singular package transform");
}
void load_lights(Reader reader, const std::array<Section, 13> &sections, Scene &scene) {
    scene.lights.reserve(sections[9].count);
    for (std::size_t i = 0; i < sections[9].count; ++i) {
        const auto offset = sections[9].offset + i * 32;
        Light light;
        light.type = reader.u32(offset);
        light.color = reader.floats<3>(offset + 4);
        light.intensity = reader.f32(offset + 16);
        light.range = reader.f32(offset + 20);
        light.inner_angle = reader.f32(offset + 24);
        light.outer_angle = reader.f32(offset + 28);
        if (light.type > 2 || light.intensity < 0 || light.range < 0 ||
            std::any_of(light.color.begin(), light.color.end(),
                        [](float v) { return v < 0 || v > 1; }) ||
            (light.type == 2 && (light.inner_angle < 0 || light.outer_angle <= light.inner_angle ||
                                 light.outer_angle > std::numbers::pi_v<float> / 2)) ||
            (light.type != 2 && (light.inner_angle != 0 || light.outer_angle != 0)))
            reject(Issue::invalid, "package punctual light values are invalid");
        scene.lights.push_back(light);
    }
    scene.light_instances.reserve(sections[10].count);
    for (std::size_t i = 0; i < sections[10].count; ++i) {
        const auto offset = sections[10].offset + i * 72;
        LightInstance instance{reader.u32(offset), reader.u32(offset + 4),
                               reader.floats<16>(offset + 8)};
        if (instance.light >= scene.lights.size() || instance.source_node >= 4096)
            reject(Issue::invalid, "package light instance reference is invalid");
        transform_valid(instance.transform);
        scene.light_instances.push_back(instance);
    }
}
void expand_instance(Scene &scene, const Instance &instance, bool first) {
    const auto &box = scene.primitives[instance.primitive].bounds;
    for (unsigned corner = 0; corner < 8; ++corner) {
        std::array<float, 3> point{};
        for (std::size_t row = 0; row < 3; ++row) {
            point[row] = instance.transform[12 + row];
            for (std::size_t column = 0; column < 3; ++column)
                point[row] +=
                    instance.transform[column * 4 + row] *
                    ((corner & (1U << column)) != 0 ? box.maximum[column] : box.minimum[column]);
            if (!std::isfinite(point[row]))
                reject(Issue::invalid, "package world bounds overflow");
        }
        if (first && corner == 0)
            scene.bounds = {point, point};
        for (std::size_t axis = 0; axis < 3; ++axis) {
            scene.bounds.minimum[axis] = std::min(scene.bounds.minimum[axis], point[axis]);
            scene.bounds.maximum[axis] = std::max(scene.bounds.maximum[axis], point[axis]);
        }
    }
}
} // namespace
PackageResult load_package(std::span<const std::byte> bytes, const std::stop_token &stop) {
    try {
        cancelled(stop);
        Reader reader{bytes};
        const auto sections = directory(reader);
        auto scene = std::make_shared<Scene>();
        scene->checksum = reader.u32(24);
        const auto provenance = reader.bytes.subspan(sections[12].offset, sections[12].bytes);
        if (std::find(provenance.begin(), provenance.end(), std::byte{0}) != provenance.end())
            reject(Issue::invalid, "NUL byte in package provenance");
        scene->provenance.assign(reinterpret_cast<const char *>(provenance.data()),
                                 provenance.size());
        load_textures(reader, sections, *scene, stop);
        load_materials(reader, sections, *scene);
        load_lights(reader, sections, *scene);
        if (sections[0].count == 0 || sections[1].count == 0 || sections[2].count == 0 ||
            sections[8].count == 0)
            reject(Issue::invalid, "package requires geometry and instances");
        scene->vertices.reserve(sections[0].count);
        for (std::size_t i = 0; i < sections[0].count; ++i) {
            if (i % 1024 == 0)
                cancelled(stop);
            const auto offset = sections[0].offset + i * 56;
            Vertex v{reader.floats<3>(offset), reader.floats<3>(offset + 12),
                     reader.floats<2>(offset + 24), reader.floats<2>(offset + 32),
                     reader.floats<4>(offset + 40)};
            const auto n2 =
                std::inner_product(v.normal.begin(), v.normal.end(), v.normal.begin(), 0.0F);
            const auto t2 = std::inner_product(v.tangent.begin(), v.tangent.begin() + 3,
                                               v.tangent.begin(), 0.0F);
            const auto dot =
                std::inner_product(v.normal.begin(), v.normal.end(), v.tangent.begin(), 0.0F);
            if (std::abs(n2 - 1) > 0.002F || std::abs(t2 - 1) > 0.002F || std::abs(dot) > 0.002F ||
                std::abs(v.tangent[3]) != 1)
                reject(Issue::invalid, "package tangent frame is not orthonormal");
            scene->vertices.push_back(v);
        }
        scene->indices.reserve(sections[1].count);
        for (std::size_t i = 0; i < sections[1].count; ++i)
            scene->indices.push_back(reader.u32(sections[1].offset + i * 4));
        scene->primitives.reserve(sections[2].count);
        for (std::size_t i = 0; i < sections[2].count; ++i) {
            const auto offset = sections[2].offset + i * 80;
            Primitive p;
            p.first_vertex = reader.u32(offset);
            p.vertex_count = reader.u32(offset + 4);
            p.material = reader.u32(offset + 8);
            const auto lod_count = reader.u32(offset + 12);
            p.bounds = {reader.floats<3>(offset + 16), reader.floats<3>(offset + 28)};
            bounds_valid(p.bounds);
            if (p.vertex_count == 0 || p.first_vertex > scene->vertices.size() ||
                p.vertex_count > scene->vertices.size() - p.first_vertex ||
                (p.material != UINT32_MAX && p.material >= scene->materials.size()) ||
                lod_count == 0 || lod_count > 3 || reader.u32(offset + 76) != 0)
                reject(Issue::invalid, "invalid primitive vertex/material/LOD range");
            for (std::size_t vertex = p.first_vertex; vertex < p.first_vertex + p.vertex_count;
                 ++vertex)
                for (std::size_t axis = 0; axis < 3; ++axis)
                    if (scene->vertices[vertex].position[axis] < p.bounds.minimum[axis] ||
                        scene->vertices[vertex].position[axis] > p.bounds.maximum[axis])
                        reject(Issue::invalid, "primitive bounds exclude vertices");
            for (std::size_t lod = 0; lod < 3; ++lod) {
                const auto record = offset + 40 + lod * 12;
                Lod level{reader.u32(record), reader.u32(record + 4), reader.f32(record + 8)};
                if (lod >= lod_count) {
                    if (level.first_index != 0 || level.index_count != 0 || level.error != 0)
                        reject(Issue::invalid, "nonzero unused LOD record");
                    continue;
                }
                if (level.index_count == 0 || level.index_count % 3 != 0 ||
                    level.first_index > scene->indices.size() ||
                    level.index_count > scene->indices.size() - level.first_index ||
                    level.error < 0 || (lod == 0 && level.error != 0) ||
                    (lod != 0 && (level.index_count > p.lods.back().index_count ||
                                  level.error < p.lods.back().error)))
                    reject(Issue::invalid, "invalid primitive LOD range or error");
                for (std::size_t index = level.first_index;
                     index < level.first_index + level.index_count; ++index)
                    if (scene->indices[index] >= p.vertex_count)
                        reject(Issue::invalid, "LOD index exceeds primitive vertices");
                p.lods.push_back(level);
            }
            scene->primitives.push_back(std::move(p));
        }
        scene->instances.reserve(sections[8].count);
        for (std::size_t i = 0; i < sections[8].count; ++i) {
            cancelled(stop);
            const auto offset = sections[8].offset + i * 72;
            Instance instance{reader.u32(offset), reader.u32(offset + 4),
                              reader.floats<16>(offset + 8)};
            if (instance.primitive >= scene->primitives.size() || instance.source_node >= 4096)
                reject(Issue::invalid, "instance references missing primitive or source node");
            transform_valid(instance.transform);
            expand_instance(*scene, instance, i == 0);
            scene->instances.push_back(instance);
        }
        cancelled(stop);
        return PackagePin{std::move(scene)};
    } catch (const Rejected &failure) {
        return failure.failure;
    } catch (const std::bad_alloc &) {
        return Failure{Issue::capacity, "package allocation failed"};
    }
}
PackageResult load_package_file(const std::filesystem::path &path, const std::stop_token &stop) {
    try {
        cancelled(stop);
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error) || error)
            return Failure{Issue::io, "package source must be a readable regular file"};
        const auto size = std::filesystem::file_size(path, error);
        if (error)
            return Failure{Issue::io, "cannot inspect package source size"};
        if (size > package_byte_limit)
            return Failure{Issue::capacity, "package file byte limit exceeded"};
        std::ifstream input(path, std::ios::binary);
        if (!input)
            return Failure{Issue::io, "cannot open package source"};
        std::vector<std::byte> bytes(static_cast<std::size_t>(size));
        constexpr std::size_t chunk_bytes = std::size_t{64} * 1024;
        for (std::size_t offset = 0; offset < bytes.size();) {
            cancelled(stop);
            const auto count = std::min(chunk_bytes, bytes.size() - offset);
            input.read(reinterpret_cast<char *>(bytes.data() + offset),
                       static_cast<std::streamsize>(count));
            if (!input || input.gcount() != static_cast<std::streamsize>(count))
                return Failure{Issue::io, "package read failed or source size changed"};
            offset += count;
        }
        if (input.peek() != std::char_traits<char>::eof() || input.bad())
            return Failure{Issue::io, "package source changed or read failed"};
        cancelled(stop);
        return load_package(bytes, stop);
    } catch (const Rejected &failure) {
        return failure.failure;
    } catch (const std::bad_alloc &) {
        return Failure{Issue::capacity, "package file allocation failed"};
    }
}
} // namespace ngen::assets
