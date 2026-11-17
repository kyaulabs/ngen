#pragma once
#include <cstdint>

namespace ngen::workload {
// Process-observed C++ new/new[] (including aligned); not C malloc or driver-private heaps.
// Disabled under sanitizers so their allocation interposition remains intact.
[[nodiscard]] std::uint64_t allocation_count() noexcept;
[[nodiscard]] bool allocation_probe_enabled() noexcept;
} // namespace ngen::workload
