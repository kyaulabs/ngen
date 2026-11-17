// Temporary art inspection: native-resolution close-ups through production owners.
// Camera-only inspection, not the fixed acceptance/performance replay.
#include "probe/capture.hpp"
#include "render/environment.hpp"
#include "render/scene_draw.hpp"
#include "scene_workload/fixture.hpp"
#include <array>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <vector>

namespace {
using namespace ngen::render;
template <class T> T take(std::variant<T, Failure> result) {
    if (const auto *error = std::get_if<Failure>(&result))
        throw std::runtime_error(error->message);
    return std::get<T>(std::move(result));
}
void wait(Context &context, const Submission &submission) {
    const auto started = SDL_GetTicks();
    while (!take(context.submission_complete(submission))) {
        if (SDL_GetTicks() - started > 10000)
            throw std::runtime_error("inspection submission timed out");
        SDL_Delay(1);
    }
}
void capture_detail(Context &context, const Image &hdr, VkExtent2D extent,
                    const std::filesystem::path &path) {
    constexpr std::size_t storage = 16U * 1024U * 1024U;
    const auto bytes = VkDeviceSize{extent.width} * extent.height * 4;
    if (extent.width != 1920 || extent.height != 1080 || bytes > storage - 1024)
        throw std::runtime_error("inspection encoding admission failed");
    auto ldr = take(
        context.create_image({extent, VK_FORMAT_R8G8B8A8_SRGB,
                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                  VK_IMAGE_USAGE_TRANSFER_SRC_BIT}));
    auto readback = take(
        context.create_buffer({bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAccess::readback}));
    wait(context, take(context.post_process({hdr, ldr, 1, true, {}, nullptr, .04F})));
    wait(context, take(context.copy_from_image({ldr, readback})));
    std::vector<std::byte> pixels(static_cast<std::size_t>(bytes));
    (void)take(readback.read(0, pixels));
    const auto surface = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>(
        SDL_CreateSurfaceFrom(static_cast<int>(extent.width), static_cast<int>(extent.height),
                              SDL_PIXELFORMAT_RGBA32, pixels.data(),
                              static_cast<int>(extent.width * 4)),
        SDL_DestroySurface);
    if (!surface)
        throw std::runtime_error("inspection surface failed");
    std::vector<std::byte> encoded(storage);
    const auto stream = std::unique_ptr<SDL_IOStream, decltype(&SDL_CloseIO)>(
        SDL_IOFromMem(encoded.data(), encoded.size()), SDL_CloseIO);
    if (!stream || !SDL_SaveBMP_IO(surface.get(), stream.get(), false))
        throw std::runtime_error("inspection encoding failed");
    const auto length = SDL_TellIO(stream.get());
    if (length <= 0 || static_cast<std::uint64_t>(length) > encoded.size())
        throw std::runtime_error("inspection encoding limit exceeded");
    (void)take(ngen::probe::write_exclusive(
        path, std::span(encoded).first(static_cast<std::size_t>(length))));
}
} // namespace
int main(int argc, char **argv) try {
    if (argc != 5)
        throw std::runtime_error("usage: detail scene.ngas side.bmp quarter.bmp rear.bmp");
    auto loaded = ngen::assets::load_package_file(argv[1]);
    if (const auto *error = std::get_if<ngen::assets::Failure>(&loaded))
        throw std::runtime_error(error->message);
    const auto package = std::get<ngen::assets::PackagePin>(std::move(loaded));
    auto context = take(Context::create({true, false, false}));
    {
        auto upload = take(SceneUpload::begin(*context, package));
        ScenePin version;
        const auto started = SDL_GetTicks();
        while (!version) {
            auto progress = take(upload->advance(*context));
            if (progress)
                version = *progress;
            if (SDL_GetTicks() - started > 60000)
                throw std::runtime_error("inspection residency timed out");
            SDL_Delay(1);
        }
        upload.reset();
        constexpr VkExtent2D extent{1920, 1080};
        auto color = take(context->create_image({extent, VK_FORMAT_R16G16B16A16_SFLOAT,
                                                 VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                                     VK_IMAGE_USAGE_SAMPLED_BIT |
                                                     VK_IMAGE_USAGE_TRANSFER_SRC_BIT}));
        auto depth = take(context->create_image(
            {extent, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT}));
        auto shadow = take(context->create_image(
            {{1024, 1024},
             VK_FORMAT_D32_SFLOAT,
             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT}));
        auto white = take(
            context->create_image({{1, 1},
                                   VK_FORMAT_R8G8B8A8_UNORM,
                                   VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT}));
        const auto upload_image = [&](const Image &image, std::span<const std::byte> bytes) {
            auto staging = take(context->create_buffer(
                {bytes.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAccess::upload}));
            (void)take(staging.write(0, bytes));
            wait(*context, take(context->copy_to_image({staging, image})));
        };
        const std::array<std::byte, 4> pixel{std::byte{255}, std::byte{255}, std::byte{255},
                                             std::byte{255}};
        upload_image(white, pixel);
        const auto generated = take(generate_environment(Sky{}));
        std::array<Image, 3> environment;
        for (std::size_t i = 0; i < environment.size(); ++i) {
            environment[i] = take(context->create_image(generated.images[i].description));
            upload_image(environment[i], generated.images[i].texels);
        }
        SceneTargets targets{color,          depth,          shadow, environment[0],
                             environment[1], environment[2], white};
        const ngen::assets::Bounds fixed_bounds{{-160, -18.9256668F, -34}, {160, 23.0743351F, 34}};
        const auto original = take(ngen::workload::make_fixture(fixed_bounds));
        auto close = original;
        close.center = {148, original.center[1], original.center[2]};
        close.radius = 35;
        SceneRenderer renderer;
        const std::array<std::uint32_t, 3> frames{0, 105, 150};
        for (std::size_t i = 0; i < frames.size(); ++i) {
            const auto baseline = take(ngen::workload::view(original, frames[i], extent));
            auto view = take(ngen::workload::view(close, frames[i], extent));
            view.light_view_projection = baseline.light_view_projection;
            wait(*context, take(renderer.draw(*context, version, view, targets)));
            (void)take(renderer.collect(*context));
            capture_detail(*context, color, extent, argv[i + 2]);
        }
    }
    if (!context->close() || context->validation_errors() != 0)
        throw std::runtime_error("inspection teardown/validation failed");
    std::puts("native_inspection=1920x1080 lod=2 exposure=1 fxaa=1 bloom=.04 camera_only=1");
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "Detail inspection: %s\n", error.what());
    return 1;
}
