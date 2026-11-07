#include "assetc/png.hpp"
#include <algorithm>
#include <array>
#include <csetjmp>
#include <cstdio>
#include <cstring>
#include <png.h>

namespace ngen::assetc {
namespace {
struct Decoder {
    std::span<const std::byte> bytes;
    std::size_t cursor{};
    png_structp reader{};
    png_infop info{};
    Image image;
    std::vector<png_bytep> rows;
    std::array<char, 256> error{};
    ImportIssue issue{ImportIssue::invalid};
    explicit Decoder(std::span<const std::byte> input) : bytes(input) {}
    Decoder(const Decoder &) = delete;
    Decoder &operator=(const Decoder &) = delete;
    ~Decoder() {
        if (reader)
            png_destroy_read_struct(&reader, info ? &info : nullptr, nullptr);
    }
};
[[noreturn]] void PNGCBAPI failed(png_structp reader, png_const_charp message) {
    auto &state = *static_cast<Decoder *>(png_get_error_ptr(reader));
    std::snprintf(state.error.data(), state.error.size(), "%s", message);
    png_longjmp(reader, 1);
}
void PNGCBAPI read_bytes(png_structp reader, png_bytep target, png_size_t count) {
    auto &state = *static_cast<Decoder *>(png_get_io_ptr(reader));
    if (count > state.bytes.size() - state.cursor)
        png_error(reader, "truncated embedded PNG");
    if (count != 0)
        std::memcpy(target, state.bytes.data() + state.cursor, count);
    state.cursor += count;
}
// Decoder owns every C++ resource OUTSIDE this setjmp frame. Between setjmp and
// any libpng longjmp there are no local nontrivial objects to skip destroying;
// mutable storage lives in the caller's state, not indeterminate automatic locals.
bool read_png(Decoder &state) {
    state.reader = png_create_read_struct(PNG_LIBPNG_VER_STRING, &state, failed, failed);
    if (!state.reader) {
        state.issue = ImportIssue::capacity;
        return false;
    }
    state.info = png_create_info_struct(state.reader);
    if (!state.info) {
        state.issue = ImportIssue::capacity;
        return false;
    }
    if (setjmp(png_jmpbuf(state.reader)))
        return false;
    png_set_read_fn(state.reader, &state, read_bytes);
    png_set_user_limits(state.reader, 8192, 8192);
    png_set_chunk_malloc_max(state.reader, std::size_t{1} * 1024 * 1024);
    png_set_chunk_cache_max(state.reader, 64);
    png_set_crc_action(state.reader, PNG_CRC_ERROR_QUIT, PNG_CRC_ERROR_QUIT);
    png_read_info(state.reader, state.info);
    if (png_get_bit_depth(state.reader, state.info) == 16) {
        state.issue = ImportIssue::unsupported;
        png_error(state.reader, "16-bit PNG sources are outside the initial subset");
    }
    const auto color = png_get_color_type(state.reader, state.info);
    const auto transparent = png_get_valid(state.reader, state.info, PNG_INFO_tRNS) != 0;
    if (color == PNG_COLOR_TYPE_PALETTE)
        png_set_palette_to_rgb(state.reader);
    if (color == PNG_COLOR_TYPE_GRAY && png_get_bit_depth(state.reader, state.info) < 8)
        png_set_expand_gray_1_2_4_to_8(state.reader);
    if (transparent)
        png_set_tRNS_to_alpha(state.reader);
    if (color == PNG_COLOR_TYPE_GRAY || color == PNG_COLOR_TYPE_GRAY_ALPHA)
        png_set_gray_to_rgb(state.reader);
    if ((static_cast<unsigned>(color) & static_cast<unsigned>(PNG_COLOR_MASK_ALPHA)) == 0 &&
        !transparent)
        png_set_add_alpha(state.reader, 255, PNG_FILLER_AFTER);
    // glTF assigns color interpretation per texture slot; do not call gamma or
    // profile conversion APIs based on PNG metadata (including linear gAMA).
    (void)png_set_interlace_handling(state.reader);
    png_read_update_info(state.reader, state.info);
    if (png_get_rowbytes(state.reader, state.info) != std::size_t{state.image.width} * 4 ||
        png_get_image_width(state.reader, state.info) != state.image.width ||
        png_get_image_height(state.reader, state.info) != state.image.height)
        png_error(state.reader, "unexpected PNG output shape");
    state.image.pixels.resize(std::size_t{state.image.width} * state.image.height * 4);
    state.rows.resize(state.image.height);
    for (std::size_t y = 0; y < state.rows.size(); ++y)
        state.rows[y] =
            reinterpret_cast<png_bytep>(state.image.pixels.data() + y * state.image.width * 4);
    png_read_image(state.reader, state.rows.data());
    png_read_end(state.reader, state.info);
    if (state.cursor != state.bytes.size())
        png_error(state.reader, "embedded PNG has trailing bytes");
    return true;
}
std::uint32_t big_endian(std::span<const std::byte> bytes) {
    std::uint32_t value = 0;
    for (auto byte : bytes)
        value = (value << 8U) | std::to_integer<std::uint32_t>(byte);
    return value;
}
} // namespace
std::variant<Image, ImportFailure> decode_png(std::span<const std::byte> bytes,
                                              std::size_t byte_allowance) {
    constexpr std::array<std::byte, 4> ihdr{std::byte{'I'}, std::byte{'H'}, std::byte{'D'},
                                            std::byte{'R'}};
    if (bytes.size() < 33 ||
        png_sig_cmp(reinterpret_cast<png_const_bytep>(bytes.data()), 0, 8) != 0 ||
        big_endian(bytes.subspan(8, 4)) != 13 ||
        !std::equal(ihdr.begin(), ihdr.end(), bytes.begin() + 12))
        return ImportFailure{ImportIssue::invalid,
                             "embedded image is not a PNG with an IHDR header"};
    const auto width = big_endian(bytes.subspan(16, 4));
    const auto height = big_endian(bytes.subspan(20, 4));
    if (width == 0 || height == 0)
        return ImportFailure{ImportIssue::invalid, "PNG dimensions must be positive"};
    if (width > 8192 || height > 8192 || std::size_t{width} * height * 4 > byte_allowance)
        return ImportFailure{ImportIssue::capacity,
                             "decoded PNG dimensions exceed the image budget"};
    Decoder state{bytes};
    state.image.width = width;
    state.image.height = height;
    if (!read_png(state))
        return ImportFailure{state.issue, "PNG decode failed: " + std::string{state.error.data()}};
    return std::move(state.image);
}
} // namespace ngen::assetc
