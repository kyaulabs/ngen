#pragma once
#include "render/context.hpp"
#include <memory>

namespace ngen::render {
struct PostDraw;
struct GraphicsUse {
    detail::ResourceKey key;
    VkImage image{};
    VkImageView view{};
    VkExtent2D extent{};
    std::uint32_t levels{};
    VkSemaphore timeline{};
    std::uint64_t serial{};
};
// Context owns this state; public resource values borrow it weakly on the owner thread.
class Resources : public std::enable_shared_from_this<Resources> {
  public:
    explicit Resources(const Context &owner);
    ~Resources();
    Resources(const Resources &) = delete;
    Resources &operator=(const Resources &) = delete;
    [[nodiscard]] std::variant<Buffer, Failure> create(BufferDescription description);
    [[nodiscard]] std::variant<Image, Failure> create(ImageDescription description);
    [[nodiscard]] VkImage image_native(detail::ResourceKey key) const noexcept;
    [[nodiscard]] VkImageView image_view(detail::ResourceKey key) const noexcept;
    [[nodiscard]] VkBuffer native(detail::ResourceKey key) const noexcept;
    [[nodiscard]] std::variant<Submission, Failure> copy(BufferCopy description);
    [[nodiscard]] std::variant<Submission, Failure> copy(ImageUpload description);
    [[nodiscard]] std::variant<Submission, Failure> copy(ImageReadback description);
    [[nodiscard]] std::variant<Submission, Failure> post(PostProcess description);
    [[nodiscard]] std::variant<Submission, Failure> mesh(MeshPass description);
    [[nodiscard]] std::variant<std::monostate, Failure> presentation_ready();
    [[nodiscard]] std::variant<GraphicsUse, Failure> prepare_frame(ImageFrame description);
    void record_frame(const PostDraw &draw) const;
    void start_frame_timing(VkCommandBuffer commands, std::size_t slot);
    void finish_frame_timing(VkCommandBuffer commands, std::size_t slot);
    [[nodiscard]] Submission submitted_frame(const GraphicsUse &use) noexcept;
    [[nodiscard]] std::variant<bool, Failure> completed(const Submission &submission);
    [[nodiscard]] std::variant<std::optional<GpuTimings>, Failure>
    timings(const Submission &submission);
    [[nodiscard]] bool timings_supported() const;
    [[nodiscard]] std::variant<std::monostate, Failure>
    write(detail::ResourceKey key, VkDeviceSize offset, std::span<const std::byte> bytes);
    [[nodiscard]] std::variant<std::monostate, Failure>
    read(detail::ResourceKey key, VkDeviceSize offset, std::span<std::byte> bytes);
    [[nodiscard]] ResourceStatistics statistics();
    void release(detail::ResourceKey key) noexcept;
    // Caller must prove device completion/loss before invalidating outstanding values.
    void close() noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace ngen::render
