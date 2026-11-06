#include "render/scene.hpp"
#include <exception>
#include <span>
#include <type_traits>

namespace ngen::render {
static_assert(sizeof(assets::Vertex) == 56 && std::is_trivially_copyable_v<assets::Vertex>);
struct SceneUpload::Impl {
    std::shared_ptr<SceneVersion> pending;
    ScenePin ready;
    Buffer staging;
    Submission submitted;
    std::size_t phase{};
    bool failed{};
};
SceneUpload::SceneUpload() : impl_(std::make_unique<Impl>()) {}
SceneUpload::~SceneUpload() = default;
std::variant<std::unique_ptr<SceneUpload>, Failure> SceneUpload::begin(Context &context,
                                                                       assets::PackagePin package) {
    try {
        if (!package || package->vertices.empty() || package->indices.empty() ||
            package->vertices.size() > assets::vertex_limit ||
            package->indices.size() > assets::index_limit)
            return Failure{"scene upload requires bounded checked geometry"};
        if (package->images.size() > resource_record_limit - 3)
            return Failure{"scene exceeds bounded native image admission"};
        auto upload = std::unique_ptr<SceneUpload>(new SceneUpload);
        auto pending = std::make_shared<SceneVersion>();
        pending->package_ = std::move(package);
        auto vertices = context.create_buffer(
            {std::as_bytes(std::span(pending->package_->vertices)).size(),
             static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT) |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT});
        if (const auto *failure = std::get_if<Failure>(&vertices))
            return *failure;
        pending->vertices_ = std::get<Buffer>(std::move(vertices));
        auto indices = context.create_buffer(
            {std::as_bytes(std::span(pending->package_->indices)).size(),
             static_cast<VkBufferUsageFlags>(VK_BUFFER_USAGE_INDEX_BUFFER_BIT) |
                 VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT});
        if (const auto *failure = std::get_if<Failure>(&indices))
            return *failure;
        pending->indices_ = std::get<Buffer>(std::move(indices));
        pending->images_.reserve(pending->package_->images.size());
        for (const auto &image : pending->package_->images) {
            if (image.mips.empty() || image.mips.size() > 32 ||
                (image.format != assets::ImageFormat::rgba8_unorm &&
                 image.format != assets::ImageFormat::rgba8_srgb))
                return Failure{"scene image is outside the native profile"};
            const auto format = image.format == assets::ImageFormat::rgba8_srgb
                                    ? VK_FORMAT_R8G8B8A8_SRGB
                                    : VK_FORMAT_R8G8B8A8_UNORM;
            auto created = context.create_image(
                {{image.mips[0].width, image.mips[0].height},
                 format,
                 static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_SRC_BIT) |
                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                 static_cast<std::uint32_t>(image.mips.size())});
            if (const auto *failure = std::get_if<Failure>(&created))
                return *failure;
            pending->images_.push_back(std::get<Image>(std::move(created)));
        }
        upload->impl_->pending = std::move(pending);
        return upload;
    } catch (const std::exception &error) {
        return Failure{error.what()};
    }
}
std::variant<std::optional<ScenePin>, Failure> SceneUpload::advance(Context &context,
                                                                    const std::stop_token &stop) {
    const auto fail = [&](Failure failure) -> std::variant<std::optional<ScenePin>, Failure> {
        impl_->failed = true;
        impl_->staging = {};
        impl_->pending.reset();
        return failure;
    };
    try {
        if (impl_->ready)
            return std::optional<ScenePin>{impl_->ready};
        if (impl_->failed)
            return Failure{"scene upload is terminal"};
        if (stop.stop_requested())
            return fail({"scene upload cancelled"});
        if (!impl_->pending->vertices_.native() || !impl_->pending->indices_.native())
            return fail({"scene resource owner is closed"});
        if (impl_->submitted.serial() != 0) {
            const auto complete = context.submission_complete(impl_->submitted);
            if (const auto *failure = std::get_if<Failure>(&complete))
                return fail(*failure);
            if (!std::get<bool>(complete))
                return std::optional<ScenePin>{};
            impl_->submitted = {};
            impl_->staging = {};
            ++impl_->phase;
        }
        if (impl_->phase == 2 + impl_->pending->images_.size()) {
            impl_->ready = std::move(impl_->pending);
            return std::optional<ScenePin>{impl_->ready};
        }
        std::span<const std::byte> bytes;
        VkDeviceSize length = 0;
        if (impl_->phase < 2) {
            bytes = impl_->phase == 0 ? std::as_bytes(std::span(impl_->pending->package_->vertices))
                                      : std::as_bytes(std::span(impl_->pending->package_->indices));
            length = bytes.size();
        } else {
            for (const auto &mip : impl_->pending->package_->images[impl_->phase - 2].mips) {
                if (mip.pixels.size() > assets::package_byte_limit - length)
                    return fail({"scene mip staging exceeds native admission"});
                length += mip.pixels.size();
            }
        }
        if (!impl_->staging.native()) {
            auto created = context.create_buffer(
                {length, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAccess::upload});
            if (const auto *failure = std::get_if<Failure>(&created))
                return fail(*failure);
            impl_->staging = std::get<Buffer>(std::move(created));
            if (impl_->phase < 2) {
                const auto written = impl_->staging.write(0, bytes);
                if (const auto *failure = std::get_if<Failure>(&written))
                    return fail(*failure);
            } else {
                VkDeviceSize offset = 0;
                for (const auto &mip : impl_->pending->package_->images[impl_->phase - 2].mips) {
                    const auto written = impl_->staging.write(offset, mip.pixels);
                    if (const auto *failure = std::get_if<Failure>(&written))
                        return fail(*failure);
                    offset += mip.pixels.size();
                }
            }
        }
        const auto copied = impl_->phase < 2
                                ? context.copy_buffer({impl_->staging,
                                                       impl_->phase == 0 ? impl_->pending->vertices_
                                                                         : impl_->pending->indices_,
                                                       length})
                                : context.copy_to_image(
                                      {impl_->staging, impl_->pending->images_[impl_->phase - 2]});
        if (const auto *failure = std::get_if<Failure>(&copied)) {
            if (failure->message == "transfer contexts are busy")
                return std::optional<ScenePin>{};
            return fail(*failure);
        }
        impl_->submitted = std::get<Submission>(copied);
        return std::optional<ScenePin>{};
    } catch (const std::exception &error) {
        return fail({error.what()});
    }
}
} // namespace ngen::render
