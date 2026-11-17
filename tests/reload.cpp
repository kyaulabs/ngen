#include "tools/reload.hpp"
#include "package_fixture.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <semaphore>
#include <stdexcept>

namespace {
using namespace std::chrono_literals;
struct Gate {
    std::binary_semaphore entered{0};
    std::mutex mutex;
    std::condition_variable_any condition;
    bool released{};
    ngen::assets::PackageResult read(const std::stop_token &stop) {
        entered.release();
        std::unique_lock lock(mutex);
        condition.wait(lock, stop, [&] { return released; });
        if (stop.stop_requested())
            return ngen::assets::Failure{ngen::assets::Issue::cancelled, "cancelled"};
        return ngen::assets::load_package(package_fixture::encode());
    }
    void release() {
        std::lock_guard lock(mutex);
        released = true;
        condition.notify_all();
    }
};
ngen::tools::ReloadedPackage completed(ngen::tools::PackageReloader &loader) {
    const auto deadline = std::chrono::steady_clock::now() + 2s;
    while (std::chrono::steady_clock::now() < deadline) {
        if (auto ready = loader.poll())
            return std::move(*ready);
        std::this_thread::yield();
    }
    throw std::runtime_error("reload did not complete within the test deadline");
}
} // namespace
TEST_CASE(
    "reload cancellation returns a typed result and a later request gets a fresh stop token") {
    Gate gate;
    ngen::tools::PackageReloader loader(
        "checked.ngas", [&](const auto &, const auto &stop) { return gate.read(stop); });
    REQUIRE(std::holds_alternative<std::monostate>(loader.request(1)));
    REQUIRE(gate.entered.try_acquire_for(2s));
    loader.cancel();
    const auto cancelled = completed(loader);
    REQUIRE(std::holds_alternative<ngen::assets::Failure>(cancelled.package));
    REQUIRE(std::get<ngen::assets::Failure>(cancelled.package).issue ==
            ngen::assets::Issue::cancelled);
    REQUIRE(std::holds_alternative<std::monostate>(loader.request(2)));
    REQUIRE(gate.entered.try_acquire_for(2s));
    gate.release();
    const auto restarted = completed(loader);
    REQUIRE(restarted.request == 2);
    REQUIRE(std::holds_alternative<ngen::assets::PackagePin>(restarted.package));
}

TEST_CASE("reload owner destruction stops and joins borrowed reader work") {
    Gate gate;
    std::atomic<bool> finished{};
    {
        ngen::tools::PackageReloader loader("checked.ngas", [&](const auto &, const auto &stop) {
            auto result = gate.read(stop);
            finished.store(true, std::memory_order_relaxed);
            return result;
        });
        REQUIRE(std::holds_alternative<std::monostate>(loader.request(1)));
        REQUIRE(gate.entered.try_acquire_for(2s));
    }
    REQUIRE(finished.load(std::memory_order_relaxed));
}

TEST_CASE("reload contains reader exceptions as typed I/O failure") {
    ngen::tools::PackageReloader loader(
        "checked.ngas", [](const auto &, const auto &) -> ngen::assets::PackageResult {
            throw std::runtime_error("reader system boundary failed");
        });
    REQUIRE(std::holds_alternative<std::monostate>(loader.request(1)));
    const auto failed = completed(loader);
    REQUIRE(std::holds_alternative<ngen::assets::Failure>(failed.package));
    const auto &error = std::get<ngen::assets::Failure>(failed.package);
    REQUIRE(error.issue == ngen::assets::Issue::io);
    REQUIRE(error.message == "package reload failed");
    REQUIRE_FALSE(loader.busy());
}

TEST_CASE("reload identifiers reject zero and cannot reuse a consumed result") {
    ngen::tools::PackageReloader loader("checked.ngas", [](const auto &, const auto &) {
        return ngen::assets::load_package(package_fixture::encode());
    });
    REQUIRE(loader.request(0) == ngen::Result<std::monostate>{ngen::Error::invalid});
    REQUIRE_FALSE(loader.busy());
    REQUIRE(std::holds_alternative<std::monostate>(loader.request(1)));
    REQUIRE(completed(loader).request == 1);
    REQUIRE(loader.request(1) == ngen::Result<std::monostate>{ngen::Error::stale});
}

TEST_CASE("package reload work stays off the owner thread and publishes one owning result") {
    Gate gate;
    ngen::tools::PackageReloader loader(
        "checked.ngas", [&](const auto &, const auto &stop) { return gate.read(stop); });
    REQUIRE(std::holds_alternative<std::monostate>(loader.request(1)));
    REQUIRE(gate.entered.try_acquire_for(2s));
    REQUIRE(loader.busy());
    REQUIRE_FALSE(loader.poll().has_value());
    REQUIRE(loader.request(2) == ngen::Result<std::monostate>{ngen::Error::busy});
    gate.release();
    auto ready = completed(loader);
    REQUIRE(ready.request == 1);
    REQUIRE(std::holds_alternative<ngen::assets::PackagePin>(ready.package));
    const auto package = std::get<ngen::assets::PackagePin>(ready.package);
    REQUIRE(package->vertices.size() == 3);
    REQUIRE_FALSE(loader.busy());
    REQUIRE_FALSE(loader.poll().has_value());
}
