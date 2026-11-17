#include "tools/reload.hpp"

namespace ngen::tools {
PackageReloader::PackageReloader(std::filesystem::path path, Reader reader)
    : path_(std::move(path)), reader_(std::move(reader)) {}
PackageReloader::~PackageReloader() = default;
Result<std::monostate> PackageReloader::request(std::uint64_t identifier) {
    if (busy_)
        return Error::busy;
    if (identifier == 0)
        return Error::invalid;
    if (identifier <= last_request_)
        return Error::stale;
    try {
        fallback_ = "package reload failed";
        result_.request = identifier;
        ready_.store(false, std::memory_order_relaxed);
        worker_ = std::jthread([this](const std::stop_token &stop) {
            try {
                result_.package = reader_(path_, stop);
            } catch (...) {
                // Preallocated before starting: even allocation failure cannot
                // escape the thread or allocate another diagnostic in this catch.
                result_.package = assets::Failure{assets::Issue::io, std::move(fallback_)};
            }
            ready_.store(true, std::memory_order_release);
        });
    } catch (...) {
        return Error::capacity;
    }
    busy_ = true;
    last_request_ = identifier;
    return std::monostate{};
}
std::optional<ReloadedPackage> PackageReloader::poll() {
    if (!busy_ || !ready_.load(std::memory_order_acquire))
        return {};
    worker_.join();
    busy_ = false;
    return std::move(result_);
}
void PackageReloader::cancel() noexcept {
    if (worker_.joinable())
        (void)worker_.request_stop();
}
} // namespace ngen::tools
