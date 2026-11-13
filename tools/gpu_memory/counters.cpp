#include "gpu_memory/counters.hpp"
#include <algorithm>
#include <charconv>
#include <limits>

namespace ngen::gpu_memory {
namespace {
char lower(char c) noexcept {
    return c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
}
bool same_name(std::string_view a, std::string_view b) noexcept {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin(),
                                              [](char x, char y) { return lower(x) == lower(y); });
}
bool take(std::string_view &text, std::string_view prefix) noexcept {
    if (text.size() < prefix.size() ||
        !std::equal(prefix.begin(), prefix.end(), text.begin(),
                    [](char a, char b) { return lower(a) == lower(b); }))
        return false;
    text.remove_prefix(prefix.size());
    return true;
}
bool key(std::string_view name, std::uint32_t pid) noexcept {
    if (!pid || pid > 2147483647U || name.empty() || name.size() > name_limit)
        return false;
    std::array<char, 32> prefix{'p', 'i', 'd', '_'};
    const auto number = std::to_chars(prefix.data() + 4, prefix.data() + prefix.size() - 1, pid);
    *number.ptr = '_';
    if (!take(name, {prefix.data(), static_cast<std::size_t>(number.ptr - prefix.data()) + 1}) ||
        !take(name, "luid_0x"))
        return false;
    const auto hex = [&] {
        std::size_t n{};
        while (n < name.size() && ((name[n] >= '0' && name[n] <= '9') ||
                                   (lower(name[n]) >= 'a' && lower(name[n]) <= 'f')))
            ++n;
        name.remove_prefix(n);
        return n != 0;
    };
    if (!hex() || !take(name, "_0x") || !hex() || !take(name, "_phys_") || name.empty())
        return false;
    return std::all_of(name.begin(), name.end(), [](char c) { return c >= '0' && c <= '9'; });
}
} // namespace
std::variant<Snapshot, Error> join(std::uint32_t pid, std::span<const Counter> dedicated,
                                   std::span<const Counter> shared,
                                   std::span<const Counter> committed) noexcept {
    if (dedicated.size() > instance_limit || shared.size() > instance_limit ||
        committed.size() > instance_limit)
        return Error::capacity;
    if (dedicated.size() != shared.size() || dedicated.size() != committed.size())
        return Error::incomplete;
    for (const auto column : {dedicated, shared, committed})
        for (std::size_t i = 0; i < column.size(); ++i) {
            const auto &entry = column[i];
            if (!key(entry.name, pid))
                return Error::instance;
            if (entry.value < 0 || entry.value > std::numeric_limits<std::int64_t>::max() / 128)
                return Error::value;
            for (std::size_t j = 0; j < i; ++j)
                if (same_name(entry.name, column[j].name))
                    return Error::duplicate;
        }
    Snapshot result;
    for (const auto &entry : dedicated) {
        if (entry.name.size() > name_limit)
            return Error::instance;
        const auto same = [&](const Counter &other) { return same_name(other.name, entry.name); };
        const auto s = std::find_if(shared.begin(), shared.end(), same);
        const auto c = std::find_if(committed.begin(), committed.end(), same);
        if (s == shared.end() || c == committed.end())
            return Error::incomplete;
        auto &row = result.instances[result.count++];
        std::copy(entry.name.begin(), entry.name.end(), row.name.begin());
        row.dedicated = static_cast<std::uint64_t>(entry.value);
        row.shared = static_cast<std::uint64_t>(s->value);
        row.committed = static_cast<std::uint64_t>(c->value);
    }
    return result;
}
} // namespace ngen::gpu_memory
