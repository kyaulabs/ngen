#include "probe/presentation.hpp"
#include "render/context.hpp"
#include "render/environment.hpp"
#include "render/mesh.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
bool failed(const auto &result, const char *operation) {
    if (const auto *failure = std::get_if<ngen::render::Failure>(&result)) {
        std::fprintf(stderr, "Vulkan resource %s: %s\n", operation, failure->message.c_str());
        return true;
    }
    return false;
}
// Verification may wait; production callers retain the same explicit bounded backpressure.
std::variant<ngen::render::Submission, ngen::render::Failure> when_ready(const auto &operation) {
    const auto started = SDL_GetTicks();
    for (;;) {
        auto result = operation();
        const auto *failure = std::get_if<ngen::render::Failure>(&result);
        if (!failure || failure->message != "transfer contexts are busy")
            return result;
        if (SDL_GetTicks() - started >= 2000)
            return ngen::render::Failure{"resource command admission timed out"};
        SDL_Delay(1);
    }
}
bool wait_submission(ngen::render::Context &context, const ngen::render::Submission &submission) {
    const auto started = SDL_GetTicks();
    for (;;) {
        const auto complete = context.submission_complete(submission);
        if (failed(complete, "completion"))
            return false;
        if (std::get<bool>(complete))
            return true;
        if (SDL_GetTicks() - started >= 2000) {
            std::fputs("Vulkan resource: transfer completion timed out\n", stderr);
            return false;
        }
        SDL_Delay(1);
    }
}
bool check_foreign(ngen::render::Context &context, const ngen::render::Buffer &readback) {
    auto creation = ngen::render::Context::create({true, false, true});
    if (failed(creation, "foreign context"))
        return false;
    const auto &other = std::get<std::unique_ptr<ngen::render::Context>>(creation);
    auto source_creation = other->create_buffer(
        {4096, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
    if (failed(source_creation, "foreign upload allocation"))
        return false;
    auto target_creation = other->create_buffer(
        {4096, VK_BUFFER_USAGE_TRANSFER_DST_BIT, ngen::render::MemoryAccess::readback});
    if (failed(target_creation, "foreign readback allocation"))
        return false;
    auto source = std::get<ngen::render::Buffer>(std::move(source_creation));
    auto target = std::get<ngen::render::Buffer>(std::move(target_creation));
    const std::array bytes{std::byte{0x92}, std::byte{0x14}, std::byte{0x38}, std::byte{0x77}};
    if (failed(source.write(0, bytes), "foreign write"))
        return false;
    if (!std::holds_alternative<ngen::render::Failure>(
            context.copy_buffer({source, readback, bytes.size()}))) {
        std::fputs("Vulkan resource: foreign resource key was accepted\n", stderr);
        return false;
    }
    const auto copied = other->copy_buffer({source, target, bytes.size()});
    if (failed(copied, "foreign copy"))
        return false;
    if (!std::holds_alternative<ngen::render::Failure>(
            context.submission_complete(std::get<ngen::render::Submission>(copied)))) {
        std::fputs("Vulkan resource: foreign submission identity was accepted\n", stderr);
        return false;
    }
    return other->close() && source.native() == VK_NULL_HANDLE && target.native() == VK_NULL_HANDLE;
}
bool check_mip_chain(ngen::render::Context &context, const ngen::render::Buffer &readback) {
    auto allocated = context.create_buffer(
        {168, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
    if (failed(allocated, "mip staging allocation"))
        return false;
    auto staging = std::get<ngen::render::Buffer>(std::move(allocated));
    std::array<std::byte, 168> pixels;
    pixels.fill(std::byte{0xee});
    if (failed(staging.write(0, pixels), "mip guard write"))
        return false;
    const auto guard = context.copy_buffer({staging, readback, pixels.size()});
    if (failed(guard, "mip readback guard") ||
        !wait_submission(context, std::get<ngen::render::Submission>(guard)))
        return false;
    for (std::size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = std::byte{static_cast<unsigned char>((i * 37U + 19U) % 256U)};
    if (failed(staging.write(0, pixels), "mip pixels"))
        return false;
    auto created =
        context.create_image({{7, 5},
                              VK_FORMAT_R8G8B8A8_UNORM,
                              static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT) |
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              3});
    if (failed(created, "mip image allocation"))
        return false;
    auto image = std::get<ngen::render::Image>(std::move(created));
    auto short_created = context.create_buffer(
        {140, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
    if (failed(short_created, "short mip staging"))
        return false;
    const auto short_staging = std::get<ngen::render::Buffer>(std::move(short_created));
    if (!std::holds_alternative<ngen::render::Failure>(
            context.copy_to_image({short_staging, image}))) {
        std::fputs("Vulkan resource: mip-chain admitted a base-level-only staging buffer\n",
                   stderr);
        return false;
    }
    const auto upload = context.copy_to_image({staging, image});
    if (failed(upload, "mip upload"))
        return false;
    const auto download = context.copy_from_image({image, readback});
    if (failed(download, "mip readback") ||
        !wait_submission(context, std::get<ngen::render::Submission>(download)))
        return false;
    std::array<std::byte, 168> actual{};
    if (failed(readback.read(0, actual), "mip pixels readback") || actual != pixels) {
        std::fputs("Vulkan resource: mip-chain GPU roundtrip pixels differ\n", stderr);
        return false;
    }
    std::puts("mip_chain_roundtrip=1 mip_levels=3 mip_bytes=168");
    return true;
}

bool check_hdr(ngen::render::Context &context, const ngen::render::Buffer &readback) {
    auto created =
        context.create_image({{3, 2},
                              VK_FORMAT_R16G16B16A16_SFLOAT,
                              static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT) |
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                  VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT});
    if (failed(created, "HDR attachment allocation"))
        return false;
    auto image = std::get<ngen::render::Image>(std::move(created));
    const std::array<std::uint16_t, 24> pixels{0x3c00, 0x4000, 0x3800, 0x3c00, 0x0000, 0x3c00,
                                               0x4200, 0x3c00, 0x4400, 0x3800, 0x4000, 0x3c00,
                                               0x3800, 0x4400, 0x0000, 0x3c00, 0x4000, 0x4200,
                                               0x4400, 0x3c00, 0x3c00, 0x3c00, 0x3c00, 0x3c00};
    auto allocated = context.create_buffer(
        {sizeof(pixels), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
    if (failed(allocated, "HDR staging allocation"))
        return false;
    auto staging = std::get<ngen::render::Buffer>(std::move(allocated));
    if (failed(staging.write(0, std::as_bytes(std::span(pixels))), "HDR staging"))
        return false;
    const auto upload = context.copy_to_image({staging, image});
    if (failed(upload, "HDR upload"))
        return false;
    const auto download = context.copy_from_image({image, readback});
    if (failed(download, "HDR readback") ||
        !wait_submission(context, std::get<ngen::render::Submission>(download)))
        return false;
    std::array<std::uint16_t, 24> actual{};
    if (failed(readback.read(0, std::as_writable_bytes(std::span(actual))), "HDR pixel bytes") ||
        actual != pixels) {
        std::fputs("Vulkan resource: HDR GPU roundtrip differs\n", stderr);
        return false;
    }
    std::puts("hdr_attachment=1 hdr_roundtrip_bytes=48");
    return true;
}

bool check_depth(ngen::render::Context &context, const ngen::render::Buffer &readback) {
    auto created =
        context.create_image({{3, 2},
                              VK_FORMAT_D32_SFLOAT,
                              static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT) |
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                                  VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT});
    if (failed(created, "depth attachment allocation"))
        return false;
    auto image = std::get<ngen::render::Image>(std::move(created));
    const std::array<float, 6> pixels{0.0F, 0.125F, 0.375F, 0.625F, 0.875F, 1.0F};
    auto allocated = context.create_buffer(
        {sizeof(pixels), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
    if (failed(allocated, "depth staging allocation"))
        return false;
    auto staging = std::get<ngen::render::Buffer>(std::move(allocated));
    if (failed(staging.write(0, std::as_bytes(std::span(pixels))), "depth staging"))
        return false;
    const auto upload = context.copy_to_image({staging, image});
    if (failed(upload, "depth upload"))
        return false;
    const auto download = context.copy_from_image({image, readback});
    if (failed(download, "depth readback") ||
        !wait_submission(context, std::get<ngen::render::Submission>(download)))
        return false;
    std::array<float, 6> actual{};
    if (failed(readback.read(0, std::as_writable_bytes(std::span(actual))), "depth pixel bytes") ||
        actual != pixels) {
        std::fputs("Vulkan resource: depth GPU roundtrip differs\n", stderr);
        return false;
    }
    std::puts("depth_attachment=1 depth_roundtrip_bytes=24");
    return true;
}

bool check_cube(ngen::render::Context &context, const ngen::render::Buffer &readback) {
    auto allocated = context.create_buffer(
        {504, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
    if (failed(allocated, "cube staging allocation"))
        return false;
    auto staging = std::get<ngen::render::Buffer>(std::move(allocated));
    std::array<std::byte, 504> pixels;
    pixels.fill(std::byte{0xee});
    if (failed(staging.write(0, pixels), "cube guard write"))
        return false;
    const auto guard = context.copy_buffer({staging, readback, pixels.size()});
    if (failed(guard, "cube readback guard") ||
        !wait_submission(context, std::get<ngen::render::Submission>(guard)))
        return false;
    for (std::size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = std::byte{static_cast<unsigned char>((i * 23U + 47U) % 256U)};
    if (failed(staging.write(0, pixels), "cube pixels"))
        return false;
    auto created =
        context.create_image({{4, 4},
                              VK_FORMAT_R8G8B8A8_UNORM,
                              static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT) |
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                              3,
                              true});
    if (failed(created, "cube image allocation"))
        return false;
    auto image = std::get<ngen::render::Image>(std::move(created));
    const auto upload = context.copy_to_image({staging, image});
    if (failed(upload, "cube upload"))
        return false;
    const auto download = context.copy_from_image({image, readback});
    if (failed(download, "cube readback") ||
        !wait_submission(context, std::get<ngen::render::Submission>(download)))
        return false;
    std::array<std::byte, 504> actual{};
    if (failed(readback.read(0, actual), "cube pixels readback") || actual != pixels) {
        std::fputs("Vulkan resource: cube mip/face GPU roundtrip pixels differ\n", stderr);
        return false;
    }
    std::puts("cube_roundtrip=1 cube_faces=6 cube_mip_levels=3 cube_bytes=504");
    return true;
}

bool check_capacity(ngen::render::Context &context) {
    const auto initial = context.resource_statistics();
    if (failed(initial, "capacity baseline"))
        return false;
    const auto baseline = std::get<ngen::render::ResourceStatistics>(initial);
    {
        std::array<ngen::render::Buffer, 63> buffers;
        for (auto &slot : buffers) {
            auto created = context.create_buffer({16, VK_BUFFER_USAGE_TRANSFER_SRC_BIT});
            if (failed(created, "capacity allocation"))
                return false;
            slot = std::get<ngen::render::Buffer>(std::move(created));
        }
        const auto excess = context.create_buffer({16, VK_BUFFER_USAGE_TRANSFER_SRC_BIT});
        const auto *rejected = std::get_if<ngen::render::Failure>(&excess);
        if (!rejected || rejected->message != "resource slot capacity exhausted") {
            std::fputs("Vulkan resource: resource records exceeded their capacity\n", stderr);
            return false;
        }
    }
    const auto reclaimed = context.resource_statistics();
    if (failed(reclaimed, "capacity reclamation"))
        return false;
    const auto after = std::get<ngen::render::ResourceStatistics>(reclaimed);
    return after.live == baseline.live && after.retiring == 0 &&
           after.allocation_bytes == baseline.allocation_bytes;
}
bool check_replacement(ngen::render::Context &context, const ngen::render::Buffer &readback) {
    const auto initial = context.resource_statistics();
    if (failed(initial, "replacement baseline"))
        return false;
    const auto baseline = std::get<ngen::render::ResourceStatistics>(initial);
    for (std::uint32_t cycle = 0; cycle < 128; ++cycle) {
        std::array<std::byte, 8> expected{};
        for (std::size_t i = 0; i < expected.size(); ++i)
            expected[i] = std::byte{static_cast<unsigned char>((cycle + 17U * i) % 256U)};
        auto created = context.create_buffer(
            {4096, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
        if (failed(created, "replacement allocation"))
            return false;
        auto upload = std::get<ngen::render::Buffer>(std::move(created));
        if (failed(upload.write(0, expected), "replacement write"))
            return false;
        const auto copied = context.copy_buffer({upload, readback, expected.size()});
        if (failed(copied, "replacement copy"))
            return false;
        upload = {};
        if (!wait_submission(context, std::get<ngen::render::Submission>(copied)))
            return false;
        std::array<std::byte, expected.size()> actual{};
        if (failed(readback.read(0, actual), "replacement readback") || actual != expected) {
            std::fputs("Vulkan resource: replacement returned stale bytes\n", stderr);
            return false;
        }
        const auto sampled = context.resource_statistics();
        if (failed(sampled, "replacement statistics"))
            return false;
        const auto statistics = std::get<ngen::render::ResourceStatistics>(sampled);
        if (statistics.live != baseline.live || statistics.retiring != 0 ||
            statistics.allocation_bytes != baseline.allocation_bytes ||
            statistics.reserved_bytes > baseline.reserved_bytes) {
            std::fputs("Vulkan resource: repeated replacement did not plateau\n", stderr);
            return false;
        }
    }
    return true;
}
bool check_image_scene(ngen::render::Context &context, const ngen::render::Buffer &readback) {
    auto created = context.create_buffer(
        {140, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
    if (failed(created, "scene staging allocation"))
        return false;
    auto staging = std::get<ngen::render::Buffer>(std::move(created));
    unsigned replacements = 0;
    ngen::render::Image presented;
    bool warmed = false;
    ngen::render::ResourceStatistics baseline{};
    ngen::render::ResourceStatistics latest{};
    const auto replace = [&] {
        std::array<std::byte, 140> expected{};
        for (std::size_t i = 0; i < expected.size(); ++i)
            expected[i] = std::byte{static_cast<unsigned char>(
                (static_cast<std::size_t>(replacements) * 31U + i * 29U + 17U) % 256U)};
        auto image_creation = context.create_image(
            {{7, 5},
             VK_FORMAT_R8G8B8A8_UNORM,
             static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT) |
                 VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT});
        if (failed(image_creation, "scene image allocation"))
            return false;
        auto image = std::get<ngen::render::Image>(std::move(image_creation));
        if (failed(staging.write(0, expected), "scene upload write"))
            return false;
        const auto uploaded = context.copy_to_image({staging, image});
        if (failed(uploaded, "scene image upload"))
            return false;
        const auto downloaded = context.copy_from_image({image, readback});
        if (failed(downloaded, "scene image readback"))
            return false;
        if (!wait_submission(context, std::get<ngen::render::Submission>(downloaded)))
            return false;
        std::array<std::byte, expected.size()> actual{};
        if (failed(readback.read(0, actual), "scene image pixels") || actual != expected) {
            std::fputs("Vulkan resource: scene replacement returned stale image pixels\n", stderr);
            return false;
        }
        presented = std::move(image);
        const auto sampled = context.resource_statistics();
        if (failed(sampled, "scene statistics"))
            return false;
        const auto statistics = std::get<ngen::render::ResourceStatistics>(sampled);
        if (warmed && (statistics.live != baseline.live || statistics.retiring != 0 ||
                       statistics.allocation_bytes != baseline.allocation_bytes ||
                       statistics.reserved_bytes > baseline.reserved_bytes)) {
            std::fputs("Vulkan resource: image replacement did not plateau across presentation\n",
                       stderr);
            return false;
        }
        if (!warmed)
            baseline = statistics;
        latest = statistics;
        ++replacements;
        return true;
    };
    if (!replace())
        return false;
    warmed = true;
#if defined(NGEN_POST_TESTS)
    const auto *source = &presented;
#else
    const ngen::render::Image *source = nullptr;
#endif
    if (!ngen::probe::presentation(context, replace, source))
        return false;
    std::printf(
        "presented_image_replacements=%u vma_allocation_bytes=%llu vma_reserved_bytes=%llu\n",
        replacements - 1, static_cast<unsigned long long>(latest.allocation_bytes),
        static_cast<unsigned long long>(latest.reserved_bytes));
    return replacements >= 97;
}
class Gate {
  public:
    explicit Gate(ngen::render::Context &context) : context_(context) {
        VkSemaphoreTypeCreateInfo type{};
        type.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
        type.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
        VkSemaphoreCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        info.pNext = &type;
        if (vkCreateSemaphore(context_.device(), &info, nullptr, &handle) != VK_SUCCESS)
            throw std::runtime_error("create resource test gate failed");
    }
    ~Gate() { (void)close(); }
    Gate(const Gate &) = delete;
    Gate &operator=(const Gate &) = delete;
    bool signal() {
        if (signaled_)
            return true;
        VkSemaphoreSignalInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
        info.semaphore = handle;
        info.value = 1;
        signaled_ = vkSignalSemaphore(context_.device(), &info) == VK_SUCCESS;
        return signaled_;
    }
    bool close() {
        if (!handle)
            return true;
        if (!signal())
            return false;
        // Only failed test cleanup may block; successful work uses timeline polling.
        if (submitted && !retired && vkQueueWaitIdle(context_.queue()) != VK_SUCCESS)
            return false;
        vkDestroySemaphore(context_.device(), handle, nullptr);
        handle = VK_NULL_HANDLE;
        return true;
    }
    VkSemaphore handle{};
    bool submitted{};
    bool retired{};

  private:
    ngen::render::Context &context_;
    bool signaled_{};
};
#if defined(NGEN_POST_TESTS)
bool check_mesh(ngen::render::Context &context, const ngen::render::Buffer &readback) {
    using namespace ngen::render;
    const auto buffer = [&](BufferDescription description) {
        auto result = context.create_buffer(description);
        if (failed(result, "mesh buffer allocation"))
            throw std::runtime_error("mesh buffer unavailable");
        return std::get<Buffer>(std::move(result));
    };
    const auto image = [&](ImageDescription description) {
        auto result = context.create_image(description);
        if (failed(result, "mesh image allocation"))
            throw std::runtime_error("mesh image unavailable");
        return std::get<Image>(std::move(result));
    };
    const auto upload = [&](const Image &target, std::span<const std::byte> bytes) {
        auto staging =
            buffer({bytes.size(), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAccess::upload});
        if (failed(staging.write(0, bytes), "mesh texture write"))
            return false;
        auto result = when_ready([&] { return context.copy_to_image({staging, target}); });
        return !failed(result, "mesh texture upload") &&
               wait_submission(context, std::get<Submission>(result));
    };
    auto white = image({{1, 1},
                        VK_FORMAT_R8G8B8A8_UNORM,
                        static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                            VK_IMAGE_USAGE_TRANSFER_DST_BIT});
    const std::array<std::byte, 4> white_pixel{std::byte{255}, std::byte{255}, std::byte{255},
                                               std::byte{255}};
    if (!upload(white, white_pixel))
        return false;
    auto environment = image({{1, 1},
                              VK_FORMAT_R8G8B8A8_UNORM,
                              static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                              1,
                              true});
    const std::array<std::byte, 24> black_faces{};
    if (!upload(environment, black_faces))
        return false;
    auto shadow = image({{1, 1},
                         VK_FORMAT_D32_SFLOAT,
                         static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_SAMPLED_BIT) |
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT});
    const float depth_one = 1;
    if (!upload(shadow, std::as_bytes(std::span{&depth_one, 1})))
        return false;
    auto color = image({{16, 16},
                        VK_FORMAT_R16G16B16A16_SFLOAT,
                        static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT});
    auto depth =
        image({{16, 16}, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT});
    std::array<shader::Vertex, 6> vertices{};
    const std::array<std::array<float, 3>, 6> positions{{{-.9F, -.8F, .7F},
                                                         {.4F, -.8F, .7F},
                                                         {-.25F, .8F, .7F},
                                                         {-.4F, -.8F, .3F},
                                                         {.9F, -.8F, .3F},
                                                         {.25F, .8F, .3F}}};
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        vertices[i].position = positions[i];
        vertices[i].normal = {0, 0, 1};
        vertices[i].tangent = {1, 0, 0, 1};
    }
    const std::array<std::uint32_t, 9> indices{0, 0, 0, 0, 1, 2, 0, 1, 2};
    auto vertex_buffer =
        buffer({sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, MemoryAccess::upload});
    auto index_buffer =
        buffer({sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, MemoryAccess::upload});
    if (failed(vertex_buffer.write(0, std::as_bytes(std::span(vertices))), "mesh vertices") ||
        failed(index_buffer.write(0, std::as_bytes(std::span(indices))), "mesh indices"))
        return false;
    std::array<MeshDraw, 2> draws{};
    const shader::Matrix identity{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
    for (auto &draw : draws) {
        draw.frame.model = draw.frame.viewProjection = draw.frame.normalMatrix =
            draw.frame.lightViewProjection = identity;
        draw.frame.cameraPosition = {0, 0, 3, 0};
        draw.frame.keyDirectionIntensity = {0, 0, -1, 0};
        draw.material.flags[2] = 1;
        draw.maps.fill(&white);
        draw.index_count = 3;
        draw.facing = MeshFacing::both;
    }
    // Near green is intentionally drawn before far red: overlap proves depth testing.
    draws[0].first_index = 6;
    draws[0].vertex_offset = 3;
    draws[0].material.emissiveAlphaCutoff = {0, 1, 0, .5F};
    draws[1].first_index = 3;
    draws[1].material.emissiveAlphaCutoff = {1, 0, 0, .5F};
    const MeshPass pass{vertex_buffer, index_buffer, color, depth, shadow,
                        environment,   environment,  white, {},    draws};
    const auto result = when_ready([&] { return context.draw_mesh(pass); });
    if (failed(result, "tracked indexed mesh draw"))
        return false;
    const auto copied = when_ready([&] { return context.copy_from_image({color, readback}); });
    if (failed(copied, "mesh readback") || !wait_submission(context, std::get<Submission>(copied)))
        return false;
    std::array<std::uint16_t, std::size_t{16} * 16 * 4> actual{};
    if (failed(readback.read(0, std::as_writable_bytes(std::span(actual))), "mesh pixels"))
        return false;
    const auto pixel = [&](std::size_t x, std::size_t y) {
        std::array<std::uint16_t, 4> value{};
        std::copy_n(actual.begin() + static_cast<std::ptrdiff_t>((y * 16 + x) * 4), 4,
                    value.begin());
        return value;
    };
    if (pixel(3, 5) != std::array<std::uint16_t, 4>{0x3c00, 0, 0, 0x3c00} ||
        pixel(12, 5) != std::array<std::uint16_t, 4>{0, 0x3c00, 0, 0x3c00} ||
        pixel(8, 5) != std::array<std::uint16_t, 4>{0, 0x3c00, 0, 0x3c00} ||
        pixel(0, 0) != std::array<std::uint16_t, 4>{0, 0, 0, 0x3c00}) {
        std::fputs("Vulkan resource: tracked mesh indexed/depth pixels differ\n", stderr);
        return false;
    }
    Sky sky;
    sky.horizon = sky.zenith = sky.ground = {.25F, .5F, 1};
    auto generated = generate_environment(sky);
    if (failed(generated, "generated environment"))
        return false;
    const auto &data = std::get<EnvironmentData>(generated);
    std::array<Image, 3> ibl;
    for (std::size_t i = 0; i < ibl.size(); ++i) {
        ibl[i] = image(data.images[i].description);
        if (!upload(ibl[i], data.images[i].texels))
            return false;
    }
    auto ibl_draw = draws[0];
    ibl_draw.material.emissiveAlphaCutoff = {0, 0, 0, .5F};
    ibl_draw.material.metallicRoughnessNormalOcclusion[1] = .045F;
    const MeshPass ibl_pass{vertex_buffer, index_buffer, color,  depth, shadow,
                            ibl[0],        ibl[1],       ibl[2], {},    std::span{&ibl_draw, 1}};
    const auto shaded = when_ready([&] { return context.draw_mesh(ibl_pass); });
    if (failed(shaded, "generated IBL draw"))
        return false;
    const auto ibl_copied = when_ready([&] { return context.copy_from_image({color, readback}); });
    if (failed(ibl_copied, "IBL readback") ||
        !wait_submission(context, std::get<Submission>(ibl_copied)) ||
        failed(readback.read(0, std::as_writable_bytes(std::span(actual))), "IBL pixels"))
        return false;
    const auto half = [](std::uint32_t bits) {
        const auto exponent = (bits >> 10U) & 31U;
        const auto mantissa = bits & 1023U;
        return exponent == 0 ? std::ldexp(static_cast<float>(mantissa), -24)
                             : std::ldexp(1 + static_cast<float>(mantissa) / 1024,
                                          static_cast<int>(exponent) - 15);
    };
    const auto shaded_pixel = pixel(12, 5);
    const std::array<float, 3> expected_ibl{.25F, .5F, 1};
    for (std::size_t channel = 0; channel < 3; ++channel)
        if (std::abs(half(shaded_pixel[channel]) - expected_ibl[channel]) > .05F) {
            std::fputs("Vulkan resource: generated cube/BRDF IBL pixels differ\n", stderr);
            return false;
        }
    ibl_draw.material.metallicRoughnessNormalOcclusion[1] = 1;
    const auto rough_shaded = when_ready([&] { return context.draw_mesh(ibl_pass); });
    if (failed(rough_shaded, "rough generated IBL draw"))
        return false;
    const auto rough_copied =
        when_ready([&] { return context.copy_from_image({color, readback}); });
    if (failed(rough_copied, "rough IBL readback") ||
        !wait_submission(context, std::get<Submission>(rough_copied)) ||
        failed(readback.read(0, std::as_writable_bytes(std::span(actual))), "rough IBL pixels"))
        return false;
    const auto rough_pixel = pixel(12, 5);
    for (std::size_t channel = 0; channel < 3; ++channel)
        if (std::abs(half(rough_pixel[channel]) - expected_ibl[channel] * (1 - std::log(2.0F))) >
            .05F) {
            std::fputs("Vulkan resource: rough generated IBL pixels differ\n", stderr);
            return false;
        }
    std::puts("generated_ibl_draw=1 generated_ibl_channels=6 generated_ibl_roughness_tail=1");
    auto invalid = pass;
    invalid.draws = {};
    if (!std::holds_alternative<Failure>(context.draw_mesh(invalid)))
        return false;
    const auto baseline_result = context.resource_statistics();
    if (failed(baseline_result, "mesh baseline"))
        return false;
    const auto baseline = std::get<ResourceStatistics>(baseline_result);
    Gate gate(context);
    auto gated = pass;
    gated.wait = {gate.handle, 1};
    const auto held = context.draw_mesh(gated);
    if (failed(held, "gated mesh draw"))
        return false;
    gate.submitted = true;
    const auto second = context.draw_mesh(pass);
    if (failed(second, "second mesh command slot"))
        return false;
    const auto busy = context.draw_mesh(pass);
    const auto *backpressure = std::get_if<Failure>(&busy);
    if (!backpressure || backpressure->message != "transfer contexts are busy")
        return false;
    const auto &last = std::get<Submission>(second);
    const auto complete = context.submission_complete(last);
    if (failed(complete, "pending mesh completion") || std::get<bool>(complete))
        return false;
    vertex_buffer = {};
    index_buffer = {};
    white = {};
    environment = {};
    shadow = {};
    depth = {};
    const auto pending_result = context.resource_statistics();
    if (failed(pending_result, "mesh pending ownership"))
        return false;
    const auto pending = std::get<ResourceStatistics>(pending_result);
    if (pending.retiring != 6 || pending.live + 6 != baseline.live ||
        pending.allocation_bytes != baseline.allocation_bytes) {
        std::fputs("Vulkan resource: mesh sources reclaimed before graphics completion\n", stderr);
        return false;
    }
    if (!gate.signal() || !wait_submission(context, last))
        return false;
    gate.retired = true;
    const auto retired_result = context.resource_statistics();
    if (failed(retired_result, "mesh reclaimed ownership"))
        return false;
    const auto retired = std::get<ResourceStatistics>(retired_result);
    if (retired.retiring != 0 || retired.allocation_bytes >= pending.allocation_bytes ||
        !gate.close())
        return false;
    std::puts(
        "tracked_indexed_pbr=1 mesh_depth=1 mesh_materials=2 mesh_retirement=1 mesh_two_slots=1");
    return true;
}
bool check_post(ngen::render::Context &context, const ngen::render::Buffer &readback) {
    auto source_created =
        context.create_image({{2, 2},
                              VK_FORMAT_R16G16B16A16_SFLOAT,
                              static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_DST_BIT) |
                                  VK_IMAGE_USAGE_SAMPLED_BIT});
    auto target_created =
        context.create_image({{2, 2},
                              VK_FORMAT_R8G8B8A8_UNORM,
                              static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT});
    if (failed(source_created, "post HDR source") || failed(target_created, "post color target"))
        return false;
    auto source = std::get<ngen::render::Image>(std::move(source_created));
    auto target = std::get<ngen::render::Image>(std::move(target_created));
    const std::array<std::uint16_t, 16> pixels{0,      0x3000, 0x3800, 0x3c00, 0x3c00, 0x4000,
                                               0x4400, 0x3c00, 0x4400, 0x3800, 0x3000, 0x3c00,
                                               0x3400, 0x3c00, 0,      0x3c00};
    const std::array<float, 12> linear{0, .125F, .5F, 1, 2, 4, 4, .5F, .125F, .25F, 1, 0};
    auto allocated = context.create_buffer(
        {sizeof(pixels), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
    if (failed(allocated, "post upload"))
        return false;
    auto staging = std::get<ngen::render::Buffer>(std::move(allocated));
    if (failed(staging.write(0, std::as_bytes(std::span(pixels))), "post source pixels") ||
        failed(context.copy_to_image({staging, source}), "post source transfer"))
        return false;
    if (!std::holds_alternative<ngen::render::Failure>(context.post_process({source, source})) ||
        !std::holds_alternative<ngen::render::Failure>(
            context.post_process({source, target, std::numeric_limits<float>::quiet_NaN()}))) {
        std::fputs("Vulkan resource: invalid post request admitted\n", stderr);
        return false;
    }
    const auto drawn = when_ready([&] { return context.post_process({source, target, 1, false}); });
    if (failed(drawn, "tracked post draw"))
        return false;
    const auto copied = when_ready([&] { return context.copy_from_image({target, readback}); });
    if (failed(copied, "post readback") ||
        !wait_submission(context, std::get<ngen::render::Submission>(copied)))
        return false;
    std::array<std::byte, 16> actual{};
    if (failed(readback.read(0, actual), "post color bytes"))
        return false;
    for (std::size_t pixel = 0; pixel < 4; ++pixel) {
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const auto x = linear[pixel * 3 + channel];
            const auto mapped = (x * (2.51F * x + .03F)) / (x * (2.43F * x + .59F) + .14F);
            const auto expected = static_cast<int>(std::lround(mapped * 255));
            if (std::abs(std::to_integer<int>(actual[pixel * 4 + channel]) - expected) > 1) {
                std::fputs("Vulkan resource: actual tone-map/orientation pixels differ\n", stderr);
                return false;
            }
        }
        if (actual[pixel * 4 + 3] != std::byte{255})
            return false;
    }
    auto srgb_created =
        context.create_image({{2, 2},
                              VK_FORMAT_R8G8B8A8_SRGB,
                              static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT});
    if (failed(srgb_created, "post sRGB target"))
        return false;
    auto srgb = std::get<ngen::render::Image>(std::move(srgb_created));
    if (failed(when_ready([&] { return context.post_process({source, srgb, .5F, false}); }),
               "post exposure/sRGB draw"))
        return false;
    const auto srgb_copy = when_ready([&] { return context.copy_from_image({srgb, readback}); });
    if (failed(srgb_copy, "post sRGB readback") ||
        !wait_submission(context, std::get<ngen::render::Submission>(srgb_copy)) ||
        failed(readback.read(0, actual), "post sRGB bytes"))
        return false;
    for (std::size_t pixel = 0; pixel < 4; ++pixel)
        for (std::size_t channel = 0; channel < 3; ++channel) {
            const auto x = linear[pixel * 3 + channel] * .5F;
            const auto mapped = (x * (2.51F * x + .03F)) / (x * (2.43F * x + .59F) + .14F);
            const auto encoded = mapped <= .0031308F
                                     ? 12.92F * mapped
                                     : 1.055F * std::pow(mapped, 1.0F / 2.4F) - .055F;
            if (std::abs(std::to_integer<int>(actual[pixel * 4 + channel]) -
                         static_cast<int>(std::lround(encoded * 255))) > 1) {
                std::fputs("Vulkan resource: post exposure or single sRGB encoding differs\n",
                           stderr);
                return false;
            }
        }
    srgb = {};
    Gate gate(context);
    const auto held = when_ready(
        [&] { return context.post_process({source, target, 1, false, {gate.handle, 1}}); });
    if (failed(held, "gated post draw"))
        return false;
    gate.submitted = true;
    const auto pending = std::get<ngen::render::Submission>(held);
    source = {};
    target = {};
    const auto statistics = context.resource_statistics();
    if (failed(statistics, "pending graphics retirement") ||
        std::get<ngen::render::ResourceStatistics>(statistics).retiring != 2) {
        std::fputs("Vulkan resource: graphics release did not retain both images\n", stderr);
        return false;
    }
    if (!gate.signal() || !wait_submission(context, pending))
        return false;
    gate.retired = true;
    const auto finished = context.resource_statistics();
    if (failed(finished, "completed graphics retirement") ||
        std::get<ngen::render::ResourceStatistics>(finished).retiring != 0 || !gate.close())
        return false;
    std::puts("tracked_post_draw=1 hdr_tone_map_pixels=4 asymmetric_orientation=1 exposure_srgb=1 "
              "graphics_retirement=1");
    return true;
}
bool check_fxaa(ngen::render::Context &context, const ngen::render::Buffer &readback) {
    auto source_created =
        context.create_image({{8, 8},
                              VK_FORMAT_R16G16B16A16_SFLOAT,
                              static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_DST_BIT) |
                                  VK_IMAGE_USAGE_SAMPLED_BIT});
    auto target_created =
        context.create_image({{8, 8},
                              VK_FORMAT_R8G8B8A8_UNORM,
                              static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) |
                                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT});
    if (failed(source_created, "AA source") || failed(target_created, "AA target"))
        return false;
    auto source = std::get<ngen::render::Image>(std::move(source_created));
    auto target = std::get<ngen::render::Image>(std::move(target_created));
    std::array<std::uint16_t, 256> pixels{};
    for (std::size_t y = 0; y < 8; ++y)
        for (std::size_t x = 0; x < 8; ++x) {
            for (std::size_t channel = 0; channel < 3; ++channel)
                pixels[(y * 8 + x) * 4 + channel] = x <= y ? 0x3c00 : 0;
            pixels[(y * 8 + x) * 4 + 3] = 0x3c00;
        }
    auto allocated = context.create_buffer(
        {sizeof(pixels), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
    if (failed(allocated, "AA upload allocation"))
        return false;
    auto staging = std::get<ngen::render::Buffer>(std::move(allocated));
    if (failed(staging.write(0, std::as_bytes(std::span(pixels))), "AA staging") ||
        failed(context.copy_to_image({staging, source}), "AA source upload"))
        return false;
    std::array<std::array<std::byte, 256>, 2> results{};
    for (std::size_t mode = 0; mode < 2; ++mode) {
        if (failed(when_ready([&] { return context.post_process({source, target, 1, mode != 0}); }),
                   "AA draw"))
            return false;
        const auto copied = when_ready([&] { return context.copy_from_image({target, readback}); });
        if (failed(copied, "AA readback") ||
            !wait_submission(context, std::get<ngen::render::Submission>(copied)) ||
            failed(readback.read(0, results[mode]), "AA pixels"))
            return false;
    }
    std::size_t softened = 0;
    for (std::size_t pixel = 0; pixel < 64; ++pixel) {
        const auto before = std::to_integer<unsigned>(results[0][pixel * 4]);
        const auto after = std::to_integer<unsigned>(results[1][pixel * 4]);
        if (after > 0 && after < before)
            ++softened;
        if (results[1][pixel * 4 + 3] != std::byte{255})
            return false;
    }
    if (softened < 4 || results[0][28] != results[1][28] || results[0][224] != results[1][224]) {
        std::fputs(
            "Vulkan resource: FXAA did not soften diagonal edges while preserving flat regions\n",
            stderr);
        return false;
    }
    std::printf("fxaa_diagonal=1 fxaa_softened_pixels=%zu fxaa_flat_regions=1\n", softened);
    return true;
}
bool check_image_presentation(ngen::render::Context &context) {
    SDL_ShowWindow(context.window());
    auto created = context.create_image({{2, 2}, VK_FORMAT_R16G16B16A16_SFLOAT});
    if (failed(created, "present HDR source"))
        return false;
    auto source = std::get<ngen::render::Image>(std::move(created));
    const std::array<std::uint16_t, 16> pixels{0x3c00, 0x3800, 0x3000, 0x3c00, 0x3000, 0x3c00,
                                               0x3800, 0x3c00, 0x3800, 0x3000, 0x3c00, 0x3c00,
                                               0x4000, 0x4000, 0x4000, 0x3c00};
    auto allocated = context.create_buffer(
        {sizeof(pixels), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
    if (failed(allocated, "present staging"))
        return false;
    auto staging = std::get<ngen::render::Buffer>(std::move(allocated));
    if (failed(staging.write(0, std::as_bytes(std::span(pixels))), "present pixels"))
        return false;
    const auto upload = context.copy_to_image({staging, source});
    if (failed(upload, "present source upload"))
        return false;
    {
        auto pending_created =
            context.create_buffer({sizeof(pixels), VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   ngen::render::MemoryAccess::readback});
        if (failed(pending_created, "external wait guard target"))
            return false;
        auto pending_target = std::get<ngen::render::Buffer>(std::move(pending_created));
        Gate external(context);
        const auto blocked = when_ready([&] {
            return context.copy_buffer(
                {staging, pending_target, sizeof(pixels), {external.handle, 1}});
        });
        if (failed(blocked, "unresolved queue dependency"))
            return false;
        external.submitted = true;
        const auto clear_rejected = context.render_frame();
        if (!std::holds_alternative<ngen::render::Failure>(clear_rejected)) {
            std::fputs("Vulkan resource: clear WSI admitted an unresolved queue dependency\n",
                       stderr);
            return false;
        }
        const auto rejected = context.render_frame({source, 1, false});
        const auto *failure = std::get_if<ngen::render::Failure>(&rejected);
        if (!failure ||
            failure->message !=
                "presentation cannot queue behind unresolved external timeline dependencies") {
            std::fputs(
                "Vulkan resource: WSI admitted an unresolved transitive timeline dependency\n",
                stderr);
            return false;
        }
        if (!external.signal() ||
            !wait_submission(context, std::get<ngen::render::Submission>(blocked)))
            return false;
        external.retired = true;
        if (!external.close())
            return false;
    }
    const auto start = SDL_GetTicks();
    ngen::render::Submission graphics;
    for (;;) {
        SDL_PumpEvents();
        const auto frame = context.render_frame({source, 1, false});
        if (failed(frame, "tracked image presentation"))
            return false;
        const auto &result = std::get<ngen::render::FrameResult>(frame);
        if (result.outcome == ngen::render::FrameOutcome::queued) {
            graphics = result.resources;
            break;
        }
        if (SDL_GetTicks() - start >= 2000) {
            std::fputs("Vulkan resource: sampled image presentation timed out\n", stderr);
            return false;
        }
        SDL_Delay(1);
    }
    if (graphics.serial() <= std::get<ngen::render::Submission>(upload).serial()) {
        std::fputs("Vulkan resource: WSI graphics use did not join the resource timeline\n",
                   stderr);
        return false;
    }
    source = {};
    if (!wait_submission(context, graphics))
        return false;
    const auto statistics = context.resource_statistics();
    if (failed(statistics, "WSI source retirement") ||
        std::get<ngen::render::ResourceStatistics>(statistics).retiring != 0)
        return false;
    std::puts("sampled_image_presentation=1 wsi_resource_timeline=1 wsi_source_retirement=1 "
              "unresolved_dependency_rejected=1");
    return true;
}
#endif
} // namespace
int main() try {
    auto creation = ngen::render::Context::create({true, false, true});
    if (const auto *failure = std::get_if<ngen::render::Failure>(&creation)) {
        std::fprintf(stderr, "Vulkan resource startup: %s\n", failure->message.c_str());
        return 1;
    }
    auto context = std::get<std::unique_ptr<ngen::render::Context>>(std::move(creation));
    Gate gate(*context);
    auto allocated = context->create_buffer(
        {4096, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
    if (const auto *failure = std::get_if<ngen::render::Failure>(&allocated)) {
        std::fprintf(stderr, "Vulkan resource allocation: %s\n", failure->message.c_str());
        return 1;
    }
    auto buffer = std::get<ngen::render::Buffer>(std::move(allocated));
    if (buffer.native() == VK_NULL_HANDLE) {
        std::fputs("Vulkan resource: allocation returned an empty buffer\n", stderr);
        return 1;
    }
    const std::array payload{std::byte{0x11}, std::byte{0x29}, std::byte{0x37}, std::byte{0x45},
                             std::byte{0x59}, std::byte{0x63}, std::byte{0x7b}, std::byte{0x8d}};
    if (failed(buffer.write(0, payload), "write"))
        return 1;
    auto target = context->create_buffer(
        {4096, VK_BUFFER_USAGE_TRANSFER_DST_BIT, ngen::render::MemoryAccess::readback});
    if (failed(target, "readback allocation"))
        return 1;
    auto readback = std::get<ngen::render::Buffer>(std::move(target));
    std::array<std::byte, payload.size()> invalid_read{};
    if (!std::holds_alternative<ngen::render::Failure>(
            context->create_buffer({0, VK_BUFFER_USAGE_TRANSFER_SRC_BIT})) ||
        !std::holds_alternative<ngen::render::Failure>(
            context->create_buffer({UINT64_MAX, VK_BUFFER_USAGE_TRANSFER_SRC_BIT})) ||
        !std::holds_alternative<ngen::render::Failure>(context->create_buffer({4096, 0})) ||
        !std::holds_alternative<ngen::render::Failure>(buffer.write(UINT64_MAX, payload)) ||
        !std::holds_alternative<ngen::render::Failure>(buffer.write(4093, payload)) ||
        !std::holds_alternative<ngen::render::Failure>(buffer.read(0, invalid_read)) ||
        !std::holds_alternative<ngen::render::Failure>(readback.write(0, payload)) ||
        !std::holds_alternative<ngen::render::Failure>(readback.read(UINT64_MAX, invalid_read)) ||
        !std::holds_alternative<ngen::render::Failure>(
            context->copy_buffer({buffer, buffer, payload.size()})) ||
        !std::holds_alternative<ngen::render::Failure>(
            context->copy_buffer({buffer, readback, 0})) ||
        !std::holds_alternative<ngen::render::Failure>(
            context->copy_buffer({buffer, readback, 4097}))) {
        std::fputs("Vulkan resource: invalid buffer request was accepted\n", stderr);
        return 1;
    }
    const auto copied = context->copy_buffer({buffer, readback, payload.size()});
    if (failed(copied, "copy"))
        return 1;
    const auto &submission = std::get<ngen::render::Submission>(copied);
    if (!wait_submission(*context, submission))
        return 1;
    std::array<std::byte, payload.size()> actual{};
    if (failed(readback.read(0, actual), "readback") || actual != payload) {
        std::fputs("Vulkan resource: GPU roundtrip data mismatch\n", stderr);
        return 1;
    }
    if (!check_foreign(*context, readback))
        return 1;
    const auto before = context->resource_statistics();
    if (failed(before, "initial statistics"))
        return 1;
    const auto first_held =
        context->copy_buffer({buffer, readback, payload.size(), {gate.handle, 1}});
    if (failed(first_held, "first gated transfer"))
        return 1;
    gate.submitted = true;
    const auto held = context->copy_buffer({buffer, readback, payload.size(), {gate.handle, 1}});
    if (failed(held, "second gated transfer"))
        return 1;
    const auto excess = context->copy_buffer({buffer, readback, payload.size(), {gate.handle, 1}});
    const auto *backpressure = std::get_if<ngen::render::Failure>(&excess);
    if (!backpressure || backpressure->message != "transfer contexts are busy") {
        std::fputs("Vulkan resource: transfer contexts exceeded their capacity\n", stderr);
        return 1;
    }
    const auto &pending_submission = std::get<ngen::render::Submission>(held);
    SDL_Delay(20);
    const auto pending_completion = context->submission_complete(pending_submission);
    if (failed(pending_completion, "pending completion") || std::get<bool>(pending_completion)) {
        std::fputs("Vulkan resource: timeline gate did not retain pending GPU work\n", stderr);
        return 1;
    }
    if (!std::holds_alternative<ngen::render::Failure>(readback.read(0, actual)) ||
        !std::holds_alternative<ngen::render::Failure>(buffer.write(0, payload))) {
        std::fputs("Vulkan resource: pending GPU memory became CPU-readable\n", stderr);
        return 1;
    }
    buffer = {};
    const auto pending_stats = context->resource_statistics();
    if (failed(pending_stats, "pending statistics"))
        return 1;
    const auto pending = std::get<ngen::render::ResourceStatistics>(pending_stats);
    if (pending.live != 1 || pending.retiring != 1 ||
        pending.allocation_bytes !=
            std::get<ngen::render::ResourceStatistics>(before).allocation_bytes) {
        std::fputs("Vulkan resource: in-flight release reclaimed memory prematurely\n", stderr);
        return 1;
    }
    if (!gate.signal() || !wait_submission(*context, pending_submission))
        return 1;
    gate.retired = true;
    const auto finished_stats = context->resource_statistics();
    if (failed(finished_stats, "retired statistics"))
        return 1;
    const auto finished = std::get<ngen::render::ResourceStatistics>(finished_stats);
    if (finished.live != 1 || finished.retiring != 0 ||
        finished.allocation_bytes >= pending.allocation_bytes ||
        failed(readback.read(0, actual), "gated readback") || actual != payload || !gate.close()) {
        std::fputs("Vulkan resource: completed transfer did not reclaim safely\n", stderr);
        return 1;
    }
#if defined(NGEN_POST_TESTS)
    if (!check_post(*context, readback) || !check_fxaa(*context, readback) ||
        !check_image_presentation(*context))
        return 1;
#endif
    if (!check_mip_chain(*context, readback) || !check_hdr(*context, readback) ||
        !check_depth(*context, readback) || !check_cube(*context, readback) ||
        !check_capacity(*context) || !check_replacement(*context, readback))
        return 1;
    if (!std::holds_alternative<ngen::render::Failure>(context->create_image(
            {{7, 5}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT}))) {
        std::fputs("Vulkan resource: transfer-only image cannot own a view\n", stderr);
        return 1;
    }
    if (!std::holds_alternative<ngen::render::Failure>(context->create_image({{0, 5}})) ||
        !std::holds_alternative<ngen::render::Failure>(
            context->create_image({{UINT32_MAX, UINT32_MAX}})) ||
        !std::holds_alternative<ngen::render::Failure>(
            context->create_image({{7, 5}, VK_FORMAT_UNDEFINED})) ||
        !std::holds_alternative<ngen::render::Failure>(
            context->create_image({{7, 5}, VK_FORMAT_R8G8B8A8_SRGB, 0}))) {
        std::fputs("Vulkan resource: invalid image request was accepted\n", stderr);
        return 1;
    }
    const std::array<ngen::render::ImageDescription, 6> invalid_images{
        {{{4, 4}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, 0},
         {{4, 4}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, 4},
         {{4, 2}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT, 1, true},
         {{4, 4}, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT},
         {{4, 4}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT},
         {{4, 4}, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT, 1, true}}};
    for (const auto &description : invalid_images)
        if (!std::holds_alternative<ngen::render::Failure>(context->create_image(description))) {
            std::fputs("Vulkan resource: incompatible mip/cube/depth image admitted\n", stderr);
            return 1;
        }
    auto image_creation =
        context->create_image({{4, 4},
                               VK_FORMAT_R8G8B8A8_SRGB,
                               static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT) |
                                   VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                               3,
                               true});
    if (failed(image_creation, "image allocation"))
        return 1;
    auto image = std::get<ngen::render::Image>(std::move(image_creation));
    if (!image.native() || !image.view()) {
        std::fputs("Vulkan resource: image ownership is empty\n", stderr);
        return 1;
    }
    if (!std::holds_alternative<ngen::render::Failure>(
            context->copy_from_image({image, readback}))) {
        std::fputs("Vulkan resource: uninitialized image became readable\n", stderr);
        return 1;
    }
    auto image_staging_creation = context->create_buffer(
        {504, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
    if (failed(image_staging_creation, "image staging allocation"))
        return 1;
    auto image_staging = std::get<ngen::render::Buffer>(std::move(image_staging_creation));
    std::array<std::byte, 504> pixels{};
    for (std::size_t i = 0; i < pixels.size(); ++i)
        pixels[i] = std::byte{static_cast<unsigned char>((i * 29 + 17) % 256)};
    if (failed(image_staging.write(0, pixels), "image staging write"))
        return 1;
    const auto uploaded = context->copy_to_image({image_staging, image});
    if (failed(uploaded, "image upload"))
        return 1;
    const auto downloaded = context->copy_from_image({image, readback});
    if (failed(downloaded, "image download") ||
        !wait_submission(*context, std::get<ngen::render::Submission>(downloaded)))
        return 1;
    std::array<std::byte, pixels.size()> actual_pixels{};
    if (failed(readback.read(0, actual_pixels), "image pixels") || actual_pixels != pixels) {
        std::fputs("Vulkan resource: image GPU roundtrip pixels differ\n", stderr);
        return 1;
    }
    Gate image_gate(*context);
    const auto held_image = context->copy_from_image({image, readback, {image_gate.handle, 1}});
    if (failed(held_image, "gated image readback"))
        return 1;
    image_gate.submitted = true;
    const auto &image_submission = std::get<ngen::render::Submission>(held_image);
    image = {};
    image_staging = {};
    auto replacement_image = context->create_image({{4, 3}});
    if (failed(replacement_image, "replacement image"))
        return 1;
    image = std::get<ngen::render::Image>(std::move(replacement_image));
    const auto held_image_stats = context->resource_statistics();
    if (failed(held_image_stats, "pending image statistics"))
        return 1;
    const auto image_pending = std::get<ngen::render::ResourceStatistics>(held_image_stats);
    if (image_pending.live != 2 || image_pending.retiring != 1) {
        std::fputs("Vulkan resource: in-flight image replacement reclaimed ownership early\n",
                   stderr);
        return 1;
    }
    if (!image_gate.signal() || !wait_submission(*context, image_submission))
        return 1;
    image_gate.retired = true;
    const auto reclaimed_image_stats = context->resource_statistics();
    if (failed(reclaimed_image_stats, "retired image statistics"))
        return 1;
    const auto image_finished = std::get<ngen::render::ResourceStatistics>(reclaimed_image_stats);
    if (image_finished.live != 2 || image_finished.retiring != 0 ||
        image_finished.allocation_bytes >= image_pending.allocation_bytes ||
        failed(readback.read(0, actual_pixels), "retired image pixels") ||
        actual_pixels != pixels || !image_gate.close()) {
        std::fputs("Vulkan resource: retired image did not preserve queued pixel data\n", stderr);
        return 1;
    }
    if (!image.native() || !image.view() || !check_image_scene(*context, readback))
        return 1;
#if defined(NGEN_POST_TESTS)
    if (!check_mesh(*context, readback))
        return 1;
#endif
    if (!context->close() || buffer.native() != VK_NULL_HANDLE ||
        readback.native() != VK_NULL_HANDLE || image.native() != VK_NULL_HANDLE ||
        image.view() != VK_NULL_HANDLE) {
        std::fputs("Vulkan resource: close did not invalidate the outstanding buffer\n", stderr);
        return 1;
    }
    if (!std::holds_alternative<ngen::render::Failure>(readback.read(0, actual_pixels)) ||
        !std::holds_alternative<ngen::render::Failure>(
            context->create_buffer({16, VK_BUFFER_USAGE_TRANSFER_SRC_BIT})) ||
        !std::holds_alternative<ngen::render::Failure>(context->create_image({{1, 1}})) ||
        !std::holds_alternative<ngen::render::Failure>(context->resource_statistics())) {
        std::fputs("Vulkan resource: closed context accepted resource access\n", stderr);
        return 1;
    }
    std::puts("buffer_lifetime=1 gpu_roundtrip=1 pending_retirement=1 resource_slots=64 "
              "replacement_cycles=128 image_lifetime=1 image_roundtrip=1 image_retirement=1 "
              "cube_retirement=1 "
              "close_invalidated=1");
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "Vulkan resource probe: %s\n", error.what());
    return 1;
}
