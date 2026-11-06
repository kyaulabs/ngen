#include "core/values.hpp"
#include <cmath>
#include <limits>
namespace ngen {
Result<Millimetres> metres(double value) {
    if (!std::isfinite(value))
        return Error::invalid;
    const double scaled = value * 1000.0;
    // Upper bound is exclusive: INT64_MAX rounds to 2^63 as a double.
    if (scaled < -0x1p63 || scaled >= 0x1p63)
        return Error::overflow;
    return Millimetres{static_cast<std::int64_t>(scaled)};
}
Result<Millimetres> add(Millimetres a, Millimetres b) {
    if ((b.value > 0 && a.value > INT64_MAX - b.value) ||
        (b.value < 0 && a.value < INT64_MIN - b.value))
        return Error::overflow;
    return Millimetres{a.value + b.value};
}
Result<std::uint64_t> next_generation(std::uint64_t value) {
    if (value == 0)
        return Error::invalid;
    if (value == UINT64_MAX)
        return Error::overflow;
    return value + 1;
}
Result<Tick> next(Tick value) {
    if (value.value == UINT64_MAX)
        return Error::overflow;
    return Tick{value.value + 1};
}
} // namespace ngen
