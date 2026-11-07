#pragma once
#include "assets/package.hpp"
#include "core/values.hpp"
#include <atomic>
#include <functional>
#include <optional>
#include <thread>

namespace ngen::tools {
struct ReloadedPackage {
    std::uint64_t request{};
    assets::PackageResult package;
};
// Owner-thread API, one dedicated I/O worker outside the simulation worker pool.
// Reader captures borrow through destruction/join. Cancellation is cooperative;
// regular-file admission is not a filesystem sandbox or a hard I/O deadline.
class PackageReloader {
  public:
    using Reader = std::function<assets::PackageResult(const std::filesystem::path &,
                                                       const std::stop_token &)>;
    explicit PackageReloader(std::filesystem::path path, Reader reader = assets::load_package_file);
    ~PackageReloader();
    PackageReloader(const PackageReloader &) = delete;
    PackageReloader &operator=(const PackageReloader &) = delete;
    [[nodiscard]] Result<std::monostate> request(std::uint64_t identifier);
    [[nodiscard]] std::optional<ReloadedPackage> poll();
    void cancel() noexcept;
    [[nodiscard]] bool busy() const noexcept { return busy_; }

  private:
    std::filesystem::path path_;
    Reader reader_;
    ReloadedPackage result_;
    std::string fallback_;
    std::atomic<bool> ready_{};
    std::uint64_t last_request_{};
    bool busy_{};
    // Destroy/join the worker before any state or borrowed reader captures.
    std::jthread worker_;
};
} // namespace ngen::tools
