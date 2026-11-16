#include "gpu_memory/counters.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>
#include <string>

TEST_CASE("GPU snapshots admit exactly the bounded instance set and reject partial gauges") {
    using namespace ngen::gpu_memory;
    std::array<std::string, instance_limit + 1> names;
    std::array<Counter, instance_limit + 1> rows;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        names[i] = "pid_42_luid_0x0_0x1_phys_" + std::to_string(i);
        rows[i] = {names[i], 1};
    }
    const auto bounded = std::span<const Counter>(rows).first(instance_limit);
    REQUIRE(std::get<Snapshot>(join(42, bounded, bounded, bounded)).count == instance_limit);
    REQUIRE(std::get<Error>(join(42, rows, rows, rows)) == Error::capacity);
    REQUIRE(std::get<Error>(join(42, bounded, bounded.first(1), bounded)) == Error::incomplete);
    const std::array unrelated{Counter{"pid_42_luid_0x0_0xffff_phys_0", 1}};
    REQUIRE(std::get<Error>(join(42, bounded.first(1), unrelated, unrelated)) == Error::incomplete);
    REQUIRE(std::get<Snapshot>(join(42, {}, {}, {})).count == 0);
}

TEST_CASE("GPU observations never count a duplicate instance twice") {
    using namespace ngen::gpu_memory;
    const std::array rows{Counter{"pid_42_luid_0x0_0xABCD_phys_0", 1},
                          Counter{"PID_42_LUID_0X0_0Xabcd_PHYS_0", 2}};
    const auto result = join(42, rows, rows, rows);
    REQUIRE(std::holds_alternative<Error>(result));
    REQUIRE(std::get<Error>(result) == Error::duplicate);
}

TEST_CASE("GPU instance keys are case-insensitive across counter columns") {
    using namespace ngen::gpu_memory;
    const std::array a{Counter{"pid_42_luid_0x0_0xABCD_phys_0", 1}};
    const std::array b{Counter{"PID_42_LUID_0X0_0Xabcd_PHYS_0", 2}};
    const auto result = join(42, a, b, a);
    REQUIRE(std::holds_alternative<Snapshot>(result));
    REQUIRE(std::get<Snapshot>(result).instances[0].shared == 2);
}

TEST_CASE("GPU gauges preserve zero and reject negative or overflowing byte values") {
    using namespace ngen::gpu_memory;
    const std::array zero{Counter{"pid_42_luid_0x0_0x1_phys_0", 0}};
    const auto idle = join(42, zero, zero, zero);
    REQUIRE(std::holds_alternative<Snapshot>(idle));
    REQUIRE(std::get<Snapshot>(idle).count == 1);
    REQUIRE(std::get<Snapshot>(idle).instances[0].dedicated == 0);
    for (auto value : {-1LL, std::numeric_limits<long long>::max()}) {
        const std::array invalid{Counter{zero[0].name, static_cast<std::int64_t>(value)}};
        const auto result = join(42, zero, invalid, zero);
        REQUIRE(std::holds_alternative<Error>(result));
        REQUIRE(std::get<Error>(result) == Error::value);
    }
}

TEST_CASE("GPU observations reject foreign, malformed or unbounded instance keys") {
    using namespace ngen::gpu_memory;
    for (const auto name :
         {"pid_420_luid_0x0_0x1_phys_0", "pid_42_unknown", "pid_42_luid_0xz_0x1_phys_0",
          "pid_42_luid_0x0_0x1_phys_-1", "pid_42_luid_0x0_0x1_phys_0_extra"}) {
        const std::array rows{Counter{name, 1}};
        const auto result = join(42, rows, rows, rows);
        REQUIRE(std::holds_alternative<Error>(result));
        REQUIRE(std::get<Error>(result) == Error::instance);
    }
}

TEST_CASE("GPU memory joins every adapter's three gauges by instance rather than row order") {
    using namespace ngen::gpu_memory;
    std::string first = "pid_42_luid_0x0_0x1234_phys_0";
    const std::string second = "pid_42_luid_0x0_0x5678_phys_1";
    const std::array dedicated{Counter{first, 128}, Counter{second, 256}};
    const std::array shared{Counter{second, 32}, Counter{first, 16}};
    const std::array committed{Counter{first, 160}, Counter{second, 320}};
    const auto result = join(42, dedicated, shared, committed);
    REQUIRE(std::holds_alternative<Snapshot>(result));
    const auto snapshot = std::get<Snapshot>(result);
    REQUIRE(snapshot.count == 2);
    first.assign("source names no longer valid");
    REQUIRE(std::string_view(snapshot.instances[0].name.data()) == "pid_42_luid_0x0_0x1234_phys_0");
    REQUIRE(snapshot.instances[0].dedicated == 128);
    REQUIRE(snapshot.instances[0].shared == 16);
    REQUIRE(snapshot.instances[0].committed == 160);
    REQUIRE(snapshot.instances[1].dedicated == 256);
    REQUIRE(snapshot.instances[1].shared == 32);
    REQUIRE(snapshot.instances[1].committed == 320);
}
