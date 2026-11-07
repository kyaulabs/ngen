#include "draw.hpp"
#include "package_fixture.hpp"
#include "render/scene.hpp"
#include <algorithm>
#include <cstdio>
#include <exception>
#include <fstream>
#include <stdexcept>

namespace {
bool failed(const auto &result, const char *operation) {
    if (const auto *failure = std::get_if<ngen::render::Failure>(&result)) {
        std::fprintf(stderr, "Scene probe %s: %s\n", operation, failure->message.c_str());
        return true;
    }
    return false;
}
std::variant<ngen::render::ScenePin, ngen::render::Failure>
finish(ngen::render::Context &context, ngen::render::SceneUpload &upload) {
    const auto start = SDL_GetTicks();
    for (;;) {
        const auto progress = upload.advance(context);
        if (const auto *failure = std::get_if<ngen::render::Failure>(&progress))
            return *failure;
        if (const auto &ready = std::get<std::optional<ngen::render::ScenePin>>(progress))
            return *ready;
        if (SDL_GetTicks() - start >= 5000)
            return ngen::render::Failure{"scene upload timed out"};
        SDL_Delay(1);
    }
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
            throw std::runtime_error("scene gate creation failed");
    }
    ~Gate() {
        if (!signal() || (used && !done && vkQueueWaitIdle(context_.queue()) != VK_SUCCESS))
            return;
        vkDestroySemaphore(context_.device(), handle, nullptr);
    }
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
    VkSemaphore handle{};
    bool used{}, done{};

  private:
    ngen::render::Context &context_;
    bool signaled_{};
};
bool cancel_inflight(ngen::render::Context &context, const ngen::render::ScenePin &active) {
    auto a = context.create_buffer(
        {16, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, ngen::render::MemoryAccess::upload});
    auto b = context.create_buffer({16, VK_BUFFER_USAGE_TRANSFER_DST_BIT});
    if (failed(a, "gate source") || failed(b, "gate target"))
        return false;
    auto source = std::get<ngen::render::Buffer>(std::move(a));
    auto target = std::get<ngen::render::Buffer>(std::move(b));
    const std::array<std::byte, 16> bytes{};
    if (failed(source.write(0, bytes), "gate bytes"))
        return false;
    auto pending = ngen::render::SceneUpload::begin(context, active->package());
    if (failed(pending, "in-flight replacement admission"))
        return false;
    auto upload = std::get<std::unique_ptr<ngen::render::SceneUpload>>(std::move(pending));
    Gate gate(context);
    if (failed(context.copy_buffer({source, target, 16, {gate.handle, 1}}), "scene gate submit"))
        return false;
    gate.used = true;
    const auto queued = upload->advance(context);
    if (failed(queued, "queued scene phase") ||
        std::get<std::optional<ngen::render::ScenePin>>(queued))
        return false;
    std::stop_source stop;
    (void)stop.request_stop();
    const auto cancelled = upload->advance(context, stop.get_token());
    if (!std::holds_alternative<ngen::render::Failure>(cancelled) || !active->vertices().native())
        return false;
    upload.reset();
    const auto held = context.resource_statistics();
    if (failed(held, "cancelled pending resources") ||
        std::get<ngen::render::ResourceStatistics>(held).retiring != 2) {
        std::fputs("Cancelled scene did not retain its submitted staging/geometry\n", stderr);
        return false;
    }
    if (!gate.signal())
        return false;
    const auto start = SDL_GetTicks();
    for (;;) {
        const auto statistics = context.resource_statistics();
        if (failed(statistics, "cancel retirement"))
            return false;
        if (std::get<ngen::render::ResourceStatistics>(statistics).retiring == 0)
            break;
        if (SDL_GetTicks() - start >= 2000)
            return false;
        SDL_Delay(1);
    }
    gate.done = true;
    std::puts("cancelled_inflight_scene=1 unpublished_version=1 active_pin_preserved=1 "
              "submitted_retirement=1");
    return true;
}
ngen::assets::PackageResult fixture() {
    const auto texture_path = std::filesystem::path{NGEN_TEST_FIXTURES} / "rgba8-srgb.ktx2";
    const auto length = std::filesystem::file_size(texture_path);
    if (length > 4096)
        return ngen::assets::Failure{ngen::assets::Issue::capacity, "synthetic texture size"};
    std::vector<std::byte> texture(static_cast<std::size_t>(length));
    std::ifstream input(texture_path, std::ios::binary);
    input.read(reinterpret_cast<char *>(texture.data()),
               static_cast<std::streamsize>(texture.size()));
    if (!input)
        return ngen::assets::Failure{ngen::assets::Issue::io, "synthetic texture read"};
    const auto bytes = package_fixture::encode(package_fixture::textured_triangle(texture));
    return ngen::assets::load_package(bytes);
}
} // namespace
int main(int argc, char **argv) try {
    if (argc > 2) {
        std::fputs("Usage: scene_probe [scene.ngas]\n", stderr);
        return 2;
    }
    auto loaded = argc == 2 ? ngen::assets::load_package_file(argv[1]) : fixture();
    if (const auto *failure = std::get_if<ngen::assets::Failure>(&loaded)) {
        std::fprintf(stderr, "Scene fixture: %s\n", failure->message.c_str());
        return 1;
    }
    auto cpu = std::get<ngen::assets::PackagePin>(loaded);
    auto creation = ngen::render::Context::create({true, false, true});
    if (failed(creation, "context"))
        return 1;
    auto context = std::get<std::unique_ptr<ngen::render::Context>>(std::move(creation));
    auto begun = ngen::render::SceneUpload::begin(*context, cpu);
    if (failed(begun, "begin checked scene upload"))
        return 1;
    cpu.reset();
    loaded = ngen::assets::Failure{ngen::assets::Issue::cancelled, "caller storage released"};
    auto upload = std::get<std::unique_ptr<ngen::render::SceneUpload>>(std::move(begun));
    const auto completed = finish(*context, *upload);
    if (failed(completed, "advance upload"))
        return 1;
    const auto version = std::get<ngen::render::ScenePin>(completed);
    upload.reset();
    if (!cancel_inflight(*context, version))
        return 1;
    auto replacement_created = ngen::render::SceneUpload::begin(*context, version->package());
    if (failed(replacement_created, "replacement while active version is pinned"))
        return 1;
    auto replacement =
        std::get<std::unique_ptr<ngen::render::SceneUpload>>(std::move(replacement_created));
    std::stop_source cancelled;
    (void)cancelled.request_stop();
    const auto rejected = replacement->advance(*context, cancelled.get_token());
    if (!std::holds_alternative<ngen::render::Failure>(rejected) || !version->vertices().native()) {
        std::fputs("Cancelled replacement changed the active immutable version\n", stderr);
        return 1;
    }
    replacement.reset();
    auto next_created = ngen::render::SceneUpload::begin(*context, version->package());
    if (failed(next_created, "second pinned version"))
        return 1;
    auto next_upload =
        std::get<std::unique_ptr<ngen::render::SceneUpload>>(std::move(next_created));
    auto next_completed = finish(*context, *next_upload);
    if (failed(next_completed, "replacement publication"))
        return 1;
    auto next = std::get<ngen::render::ScenePin>(std::move(next_completed));
    next_upload.reset();
    if (next == version || next->vertices().native() == version->vertices().native() ||
        !version->vertices().native()) {
        std::fputs("Pinned versions aliased native geometry\n", stderr);
        return 1;
    }
    const auto resident_statistics = context->resource_statistics();
    if (failed(resident_statistics, "two-version statistics"))
        return 1;
    std::printf(
        "pinned_versions=2 resident_allocation_bytes=%llu resident_reserved_bytes=%llu\n",
        static_cast<unsigned long long>(
            std::get<ngen::render::ResourceStatistics>(resident_statistics).allocation_bytes),
        static_cast<unsigned long long>(
            std::get<ngen::render::ResourceStatistics>(resident_statistics).reserved_bytes));
    const auto start = SDL_GetTicks();
    if (version->package()->vertices.empty() || version->package()->instances.empty() ||
        !version->vertices().native() || !version->indices().native()) {
        std::fputs("Published scene lost owning geometry/version data\n", stderr);
        return 1;
    }
    const auto expected = std::as_bytes(std::span(version->package()->vertices));
    VkDeviceSize readback_size = std::max<VkDeviceSize>(
        expected.size(), std::as_bytes(std::span(version->package()->indices)).size());
    for (const auto &image : version->package()->images) {
        VkDeviceSize bytes = 0;
        for (const auto &mip : image.mips)
            bytes += mip.pixels.size();
        readback_size = std::max(readback_size, bytes);
    }
    auto allocated = context->create_buffer(
        {readback_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT, ngen::render::MemoryAccess::readback});
    if (failed(allocated, "readback buffer"))
        return 1;
    auto readback = std::get<ngen::render::Buffer>(std::move(allocated));
    const auto copied = context->copy_buffer({version->vertices(), readback, expected.size()});
    if (failed(copied, "resident vertex readback"))
        return 1;
    for (;;) {
        const auto complete =
            context->submission_complete(std::get<ngen::render::Submission>(copied));
        if (failed(complete, "resident completion"))
            return 1;
        if (std::get<bool>(complete))
            break;
        if (SDL_GetTicks() - start >= 5000)
            return 1;
        SDL_Delay(1);
    }
    std::vector<std::byte> actual(expected.size());
    if (failed(readback.read(0, actual), "resident vertex bytes") ||
        !std::equal(actual.begin(), actual.end(), expected.begin(), expected.end())) {
        std::fputs("Resident geometry GPU bytes differ\n", stderr);
        return 1;
    }
    const auto expected_indices = std::as_bytes(std::span(version->package()->indices));
    const auto index_copy =
        context->copy_buffer({version->indices(), readback, expected_indices.size()});
    if (failed(index_copy, "resident index readback"))
        return 1;
    for (;;) {
        const auto complete =
            context->submission_complete(std::get<ngen::render::Submission>(index_copy));
        if (failed(complete, "resident index completion"))
            return 1;
        if (std::get<bool>(complete))
            break;
        if (SDL_GetTicks() - start >= 5000)
            return 1;
        SDL_Delay(1);
    }
    std::vector<std::byte> actual_indices(expected_indices.size());
    if (failed(readback.read(0, actual_indices), "resident index bytes") ||
        !std::equal(actual_indices.begin(), actual_indices.end(), expected_indices.begin(),
                    expected_indices.end())) {
        std::fputs("Resident GPU indices differ\n", stderr);
        return 1;
    }
    if (version->images().size() != version->package()->images.size() ||
        version->images().empty()) {
        std::fputs("Resident scene lost its image\n", stderr);
        return 1;
    }
    std::uint64_t verified_mip_bytes = 0;
    for (std::size_t image = 0; image < version->images().size(); ++image) {
        const auto image_copy = context->copy_from_image({version->images()[image], readback});
        if (failed(image_copy, "resident image readback"))
            return 1;
        for (;;) {
            const auto complete =
                context->submission_complete(std::get<ngen::render::Submission>(image_copy));
            if (failed(complete, "resident mip completion"))
                return 1;
            if (std::get<bool>(complete))
                break;
            if (SDL_GetTicks() - start >= 5000)
                return 1;
            SDL_Delay(1);
        }
        std::vector<std::byte> expected_mips;
        for (const auto &mip : version->package()->images[image].mips)
            expected_mips.insert(expected_mips.end(), mip.pixels.begin(), mip.pixels.end());
        std::vector<std::byte> actual_mips(expected_mips.size());
        if (failed(readback.read(0, actual_mips), "resident image pixels") ||
            actual_mips != expected_mips) {
            std::fputs("Resident mip bytes differ\n", stderr);
            return 1;
        }
        verified_mip_bytes += expected_mips.size();
    }
    std::printf("resident_verified_mip_bytes=%llu\n",
                static_cast<unsigned long long>(verified_mip_bytes));
    if (!ngen::probe::draw_scene(*context, next, version))
        return 1;
    if (!context->close() || version->vertices().native() || version->indices().native() ||
        version->images()[0].native())
        return 1;
    std::puts("checked_scene_upload=1 immutable_version=1 owning_cpu_pin=1 "
              "resident_vertex_readback=1 resident_index_readback=1 resident_mip_readback=1 "
              "cancelled_replacement=1 "
              "close_invalidated=1");
    std::printf(
        "resident_vertices=%zu resident_indices=%zu resident_images=%zu retained_lights=%zu\n",
        version->package()->vertices.size(), version->package()->indices.size(),
        version->package()->images.size(), version->package()->lights.size());
    std::array<std::size_t, 3> kinds{};
    for (const auto &light : version->package()->lights)
        ++kinds[light.type];
    std::size_t uv1_maps = 0;
    for (const auto &material : version->package()->materials)
        for (const auto &map : material.maps)
            if (map.texture != UINT32_MAX && map.uv_set == 1)
                ++uv1_maps;
    std::printf("source_directional=%zu source_point=%zu source_spot=%zu source_uv1_maps=%zu\n",
                kinds[0], kinds[1], kinds[2], uv1_maps);
    std::array<std::size_t, 3> alpha{};
    std::size_t two_sided = 0;
    for (const auto &material : version->package()->materials) {
        ++alpha[material.alpha];
        two_sided += material.double_sided ? 1U : 0U;
    }
    std::printf("source_opaque=%zu source_mask=%zu source_blend=%zu source_double_sided=%zu\n",
                alpha[0], alpha[1], alpha[2], two_sided);
    return 0;
} catch (const std::exception &error) {
    std::fprintf(stderr, "Scene probe: %s\n", error.what());
    return 1;
}
