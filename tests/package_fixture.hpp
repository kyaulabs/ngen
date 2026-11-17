#pragma once
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include <zlib.h>

namespace package_fixture {
inline void u32(std::vector<std::byte> &out, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<std::byte>((value >> shift) & 255U));
}
inline void u64(std::vector<std::byte> &out, std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8)
        out.push_back(static_cast<std::byte>((value >> shift) & 255U));
}
inline void f32(std::vector<std::byte> &out, float value) {
    u32(out, std::bit_cast<std::uint32_t>(value));
}
inline void set32(std::vector<std::byte> &out, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        out[offset + i] = static_cast<std::byte>((value >> (i * 8U)) & 255U);
}
inline void set64(std::vector<std::byte> &out, std::size_t offset, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i)
        out[offset + i] = static_cast<std::byte>((value >> (i * 8U)) & 255U);
}
inline void checksum(std::vector<std::byte> &out) {
    set32(out, 24,
          static_cast<std::uint32_t>(
              crc32_z(0, reinterpret_cast<const Bytef *>(out.data() + 64), out.size() - 64)));
}
using Sections = std::array<std::vector<std::byte>, 13>;
inline Sections triangle() {
    Sections sections;
    const std::array<std::array<float, 14>, 3> vertices{
        {{-1, 0, 0, 0, 0, 1, 0, 0, .25F, .5F, 1, 0, 0, 1},
         {1, 0, 0, 0, 0, 1, 1, 0, .5F, .5F, 1, 0, 0, 1},
         {0, 2, 0, 0, 0, 1, .5F, 1, .75F, .5F, 1, 0, 0, 1}}};
    for (const auto &vertex : vertices)
        for (float v : vertex)
            f32(sections[0], v);
    for (auto index : std::array<std::uint32_t, 3>{0, 1, 2})
        u32(sections[1], index);
    auto &primitive = sections[2];
    for (auto value : std::array<std::uint32_t, 4>{0, 3, UINT32_MAX, 1})
        u32(primitive, value);
    for (float value : std::array<float, 6>{-1, 0, 0, 1, 2, 0})
        f32(primitive, value);
    u32(primitive, 0);
    u32(primitive, 3);
    f32(primitive, 0);
    primitive.resize(80, std::byte{0});
    auto &instance = sections[8];
    u32(instance, 0);
    u32(instance, 7);
    for (float value : std::array<float, 16>{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 4, 5, 6, 1})
        f32(instance, value);
    return sections;
}
inline Sections textured_triangle(std::span<const std::byte> texture, bool linear = false) {
    auto sections = triangle();
    auto &record = sections[6];
    u64(record, 0);
    u64(record, texture.size());
    for (auto value : std::array<std::uint32_t, 4>{2, 2, 2, linear ? 1U : 2U})
        u32(record, value);
    sections[7].assign(texture.begin(), texture.end());
    u32(sections[4], 0);
    u32(sections[4], 0);
    for (auto value : std::array<std::uint32_t, 4>{9987, 9729, 33071, 10497})
        u32(sections[5], value);
    return sections;
}
inline std::vector<std::byte> encode(const Sections &sections = triangle()) {
    constexpr std::array<std::uint32_t, 13> strides{56, 4, 80, 128, 8, 16, 32, 1, 72, 32, 72, 1, 1};
    std::vector<std::byte> out(480, std::byte{0});
    constexpr std::array<char, 8> magic{'N', 'G', 'A', 'S', 'S', 'E', 'T', '\0'};
    for (std::size_t i = 0; i < magic.size(); ++i)
        out[i] = static_cast<std::byte>(magic[i]);
    set32(out, 8, 1);
    set32(out, 12, 64);
    set32(out, 28, 13);
    for (std::size_t i = 0; i < sections.size(); ++i) {
        while (out.size() % 8 != 0)
            out.push_back(std::byte{0});
        const auto offset = 64 + i * 32;
        set32(out, offset, static_cast<std::uint32_t>(i + 1));
        set32(out, offset + 4, strides[i]);
        set32(out, offset + 8, static_cast<std::uint32_t>(sections[i].size() / strides[i]));
        set64(out, offset + 16, out.size());
        set64(out, offset + 24, sections[i].size());
        out.insert(out.end(), sections[i].begin(), sections[i].end());
    }
    set64(out, 16, out.size());
    checksum(out);
    return out;
}
} // namespace package_fixture
