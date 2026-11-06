#pragma once
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <variant>
#include <vulkan/vulkan.h>

namespace ngen::render {
struct Failure {
    std::string message;
};
class Resources;
class Submission {
  public:
    Submission() = default;
    [[nodiscard]] std::uint64_t serial() const { return value_; }

  private:
    friend class Resources;
    Submission(std::weak_ptr<Resources> owner, std::uint64_t value)
        : owner_(std::move(owner)), value_(value) {}
    std::weak_ptr<Resources> owner_;
    std::uint64_t value_{};
};
enum class MemoryAccess : std::uint8_t { device, upload, readback };
struct BufferDescription {
    VkDeviceSize bytes{};
    VkBufferUsageFlags usage{};
    MemoryAccess memory{MemoryAccess::device};
};
namespace detail {
struct ResourceKey {
    std::uint32_t slot{UINT32_MAX};
    std::uint32_t generation{};
};
} // namespace detail
// Move, release and access on the Context owner thread. Native handles are borrowed;
// submit uses through Context tracking, not independent native queue submissions.
class Buffer {
  public:
    Buffer() = default;
    ~Buffer();
    Buffer(Buffer &&) noexcept;
    Buffer &operator=(Buffer &&) noexcept;
    Buffer(const Buffer &) = delete;
    Buffer &operator=(const Buffer &) = delete;
    // Borrowed owner-thread access; null after release or context teardown.
    [[nodiscard]] VkBuffer native() const;
    [[nodiscard]] std::variant<std::monostate, Failure> write(VkDeviceSize offset,
                                                              std::span<const std::byte> bytes);
    [[nodiscard]] std::variant<std::monostate, Failure> read(VkDeviceSize offset,
                                                             std::span<std::byte> bytes) const;

  private:
    friend class Resources;
    Buffer(std::weak_ptr<Resources> owner, detail::ResourceKey key);
    void release() noexcept;
    std::weak_ptr<Resources> owner_;
    detail::ResourceKey key_;
};
struct ImageDescription {
    VkExtent2D extent{};
    VkFormat format{VK_FORMAT_R8G8B8A8_SRGB};
    VkImageUsageFlags usage{static_cast<VkImageUsageFlags>(VK_IMAGE_USAGE_TRANSFER_DST_BIT) |
                            VK_IMAGE_USAGE_SAMPLED_BIT};
    // 1..full chain; color formats RGBA8 sRGB/UNORM, RGBA16F, or 2D D32 depth.
    std::uint32_t mip_levels{1};
    bool cube{false}; // Six square color faces and one owned cube view.
};
// Same owner-thread/borrowing contract as Buffer; whole-image layout/last-use tracking.
class Image {
  public:
    Image() = default;
    ~Image();
    Image(Image &&) noexcept;
    Image &operator=(Image &&) noexcept;
    Image(const Image &) = delete;
    Image &operator=(const Image &) = delete;
    [[nodiscard]] VkImage native() const;
    [[nodiscard]] VkImageView view() const;

  private:
    friend class Resources;
    Image(std::weak_ptr<Resources> owner, detail::ResourceKey key);
    void release() noexcept;
    std::weak_ptr<Resources> owner_;
    detail::ResourceKey key_;
};
// Collection is nonblocking. Bytes cover VMA allocations/reservations, not WSI or driver memory.
struct ResourceStatistics {
    std::uint32_t live{};
    std::uint32_t retiring{};
    std::uint64_t allocation_bytes{};
    std::uint64_t reserved_bytes{};
};
struct TimelineWait {
    VkSemaphore semaphore{}; // Borrowed timeline; caller retains it through submission completion.
    std::uint64_t value{};
};
struct BufferCopy {
    const Buffer &source;
    const Buffer &destination;
    VkDeviceSize bytes{};
    TimelineWait wait{};
};
// Whole-image tight packing: increasing mip, then cube face (+X,-X,+Y,-Y,+Z,-Z).
// Transfer buffers must cover every requested mip/face in the native texel format.
struct ImageUpload {
    const Buffer &source;
    const Image &destination;
    TimelineWait wait{};
};
struct ImageReadback {
    const Image &source;
    const Buffer &destination;
    TimelineWait wait{};
};
// Distinct initialized filterable 2D source and single-mip RGBA8 color target.
// Returns explicit backpressure; no owner-thread waiting or ordinary device idle.
struct PostProcess {
    const Image &source;
    const Image &destination;
    float exposure{1}; // Finite linear multiplier in 0..64.
    bool fxaa{true};
    TimelineWait wait{};
};
} // namespace ngen::render
