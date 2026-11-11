#include "probe/capture.hpp"
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace ngen::probe {
namespace {
using namespace render;
void require(bool value, const char *message) {
    if (!value)
        throw std::runtime_error(message);
}
template <class T> T take(std::variant<T, Failure> result) {
    if (const auto *failure = std::get_if<Failure>(&result))
        throw std::runtime_error(failure->message);
    return std::get<T>(std::move(result));
}
void wait(Context &context, const Submission &submission) {
    const auto started = SDL_GetTicks();
    while (!take(context.submission_complete(submission))) {
        require(SDL_GetTicks() - started < 5000, "capture completion timed out");
        SDL_Delay(1);
    }
}
} // namespace
std::variant<std::monostate, render::Failure>
write_exclusive(const std::filesystem::path &path, std::span<const std::byte> bytes) try {
    std::FILE *file{};
#if defined(_WIN32)
    (void)fopen_s(&file, path.string().c_str(), "wbx");
#else
    file = std::fopen(path.string().c_str(), "wbx");
#endif
    const auto close_file = [](std::FILE *value) { (void)std::fclose(value); };
    auto output = std::unique_ptr<std::FILE, decltype(close_file)>(file, close_file);
    require(output != nullptr, "scene capture path exists or cannot be created");
    const auto written = std::fwrite(bytes.data(), 1, bytes.size(), output.get());
    const bool closed = std::fclose(output.release()) == 0;
    require(written == bytes.size() && closed, "scene capture write/close failed");
    return std::monostate{};
} catch (const std::exception &error) {
    return render::Failure{error.what()};
}
std::variant<std::monostate, render::Failure>
capture(render::Context &context, const render::Image &hdr, VkExtent2D extent,
        const std::filesystem::path &path, float exposure, bool fxaa,
        const render::Overlay *overlay, float bloom) try {
    constexpr auto capacity = std::size_t{8} * 1024 * 1024;
    const auto bytes = VkDeviceSize{extent.width} * extent.height * 4;
    require(extent.width > 0 && extent.height > 0 && extent.width <= 8192 &&
                extent.height <= 8192 && bytes <= capacity - 1024,
            "scene capture extent exceeds bounded encoding storage");
    auto ldr = take(
        context.create_image({extent, VK_FORMAT_R8G8B8A8_SRGB,
                              static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT}));
    auto readback = take(
        context.create_buffer({bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAccess::readback}));
    wait(context, take(context.post_process({hdr, ldr, exposure, fxaa, {}, overlay, bloom})));
    wait(context, take(context.copy_from_image({ldr, readback})));
    std::vector<std::byte> pixels(static_cast<std::size_t>(bytes));
    (void)take(readback.read(0, pixels));
    const auto surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>(
        SDL_CreateSurfaceFrom(static_cast<int>(extent.width), static_cast<int>(extent.height),
                              SDL_PIXELFORMAT_RGBA32, pixels.data(),
                              static_cast<int>(extent.width * 4)),
        SDL_DestroySurface);
    require(surface != nullptr, "scene capture surface creation failed");
    std::vector<std::byte> encoded(capacity);
    const auto stream = std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)>(
        SDL_IOFromMem(encoded.data(), encoded.size()), SDL_CloseIO);
    require(stream != nullptr && SDL_SaveBMP_IO(surface.get(), stream.get(), false),
            "scene post-processed BMP encoding failed");
    const auto length = SDL_TellIO(stream.get());
    require(length > 0 && static_cast<std::uint64_t>(length) <= encoded.size(),
            "scene BMP capture exceeded its bounded encoding storage");
    return write_exclusive(path, std::span(encoded).first(static_cast<std::size_t>(length)));
} catch (const std::exception &error) {
    return render::Failure{error.what()};
}
} // namespace ngen::probe
