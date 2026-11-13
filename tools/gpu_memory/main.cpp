#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "gpu_memory/counters.hpp"
#include <array>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <pdh.h>
#include <pdhmsg.h>
#include <string_view>
#include <windows.h>

namespace {
using namespace ngen::gpu_memory;
struct Column {
    alignas(PDH_FMT_COUNTERVALUE_ITEM_W) std::array<std::byte, 65536> storage{};
    std::array<std::array<char, name_limit + 1>, instance_limit> names{};
    std::array<Counter, instance_limit> values{};
    std::size_t count{};
};
class Query {
  public:
    ~Query() {
        if (query_)
            (void)PdhCloseQuery(query_);
    }
    Query() = default;
    Query(const Query &) = delete;
    Query &operator=(const Query &) = delete;
    DWORD open(std::uint32_t pid) {
        PDH_HQUERY next{};
        auto status = PdhOpenQueryW(nullptr, 0, &next);
        if (status != ERROR_SUCCESS)
            return static_cast<DWORD>(status);
        query_ = next;
        const std::array names{L"Dedicated Usage", L"Shared Usage", L"Total Committed"};
        for (std::size_t i = 0; i < names.size(); ++i) {
            std::array<wchar_t, 192> path{};
            const auto length =
                std::swprintf(path.data(), path.size(), LR"(\GPU Process Memory(pid_%u_*)\%ls)",
                              static_cast<unsigned>(pid), names[i]);
            if (length <= 0 || static_cast<std::size_t>(length) >= path.size())
                return static_cast<DWORD>(PDH_INVALID_PATH);
            status = PdhAddEnglishCounterW(query_, path.data(), 0, &counters_[i]);
            if (status != ERROR_SUCCESS)
                return static_cast<DWORD>(status);
        }
        return ERROR_SUCCESS;
    }
    DWORD collect() { return static_cast<DWORD>(PdhCollectQueryData(query_)); }
    DWORD read(std::size_t index, Column &column) {
        DWORD bytes = static_cast<DWORD>(column.storage.size()), count{};
        const auto status = static_cast<DWORD>(PdhGetFormattedCounterArrayW(
            counters_[index], PDH_FMT_LARGE | PDH_FMT_NOSCALE | PDH_FMT_NOCAP100, &bytes, &count,
            reinterpret_cast<PPDH_FMT_COUNTERVALUE_ITEM_W>(column.storage.data())));
        if (status == static_cast<DWORD>(PDH_NO_DATA))
            return ERROR_SUCCESS;
        if (status != ERROR_SUCCESS)
            return status;
        if (count > instance_limit || bytes > column.storage.size() ||
            static_cast<std::size_t>(count) * sizeof(PDH_FMT_COUNTERVALUE_ITEM_W) > bytes)
            return static_cast<DWORD>(PDH_INVALID_DATA);
        const auto begin = reinterpret_cast<std::uintptr_t>(column.storage.data());
        const auto end = begin + bytes;
        for (DWORD i = 0; i < count; ++i) {
            PDH_FMT_COUNTERVALUE_ITEM_W item{};
            std::memcpy(&item, column.storage.data() + i * sizeof(item), sizeof(item));
            if (item.FmtValue.CStatus != PDH_CSTATUS_VALID_DATA &&
                item.FmtValue.CStatus != PDH_CSTATUS_NEW_DATA)
                return item.FmtValue.CStatus;
            auto name = reinterpret_cast<std::uintptr_t>(item.szName);
            if (name < begin || name >= end)
                return static_cast<DWORD>(PDH_INVALID_DATA);
            bool terminated = false;
            for (std::size_t n = 0; n <= name_limit && name <= end && end - name >= sizeof(wchar_t);
                 ++n) {
                wchar_t c{};
                std::memcpy(&c, reinterpret_cast<const void *>(name), sizeof(c));
                name += sizeof(c);
                if (c == 0) {
                    terminated = true;
                    break;
                }
                if (n == name_limit || c > 127)
                    return static_cast<DWORD>(PDH_INVALID_DATA);
                column.names[i][n] = static_cast<char>(c);
            }
            if (!terminated)
                return static_cast<DWORD>(PDH_INVALID_DATA);
            column.values[i] = {column.names[i].data(), item.FmtValue.largeValue};
        }
        column.count = count;
        return ERROR_SUCCESS;
    }

  private:
    PDH_HQUERY query_{};
    std::array<PDH_HCOUNTER, 3> counters_{};
};
void error(DWORD status, const char *kind) {
    std::printf("{\"status\":\"error\",\"pdh_status\":%lu,\"kind\":\"%s\",\"rows\":[]}\n",
                static_cast<unsigned long>(status), kind);
}
void read(Query &query, std::uint32_t pid) {
    const auto collected = query.collect();
    if (collected == static_cast<DWORD>(PDH_NO_DATA)) {
        std::puts("{\"status\":\"no_own_instances\",\"pdh_status\":0,\"rows\":[]}");
        return;
    }
    if (collected != ERROR_SUCCESS) {
        error(collected, "collect");
        return;
    }
    std::array<Column, 3> columns{};
    for (std::size_t i = 0; i < columns.size(); ++i) {
        const auto status = query.read(i, columns[i]);
        if (status != ERROR_SUCCESS) {
            error(status, "counter");
            return;
        }
    }
    const auto result = join(pid, std::span(columns[0].values).first(columns[0].count),
                             std::span(columns[1].values).first(columns[1].count),
                             std::span(columns[2].values).first(columns[2].count));
    if (std::holds_alternative<Error>(result)) {
        error(0, "invalid_counter_set");
        return;
    }
    const auto &snapshot = std::get<Snapshot>(result);
    std::printf("{\"status\":\"%s\",\"pdh_status\":0,\"rows\":[",
                snapshot.count ? "observed" : "no_own_instances");
    for (std::size_t i = 0; i < snapshot.count; ++i) {
        const auto &row = snapshot.instances[i];
        std::printf("%s{\"Name\":\"%s\",\"DedicatedUsage\":%llu,\"SharedUsage\":%llu,"
                    "\"TotalCommitted\":%llu}",
                    i ? "," : "", row.name.data(), static_cast<unsigned long long>(row.dedicated),
                    static_cast<unsigned long long>(row.shared),
                    static_cast<unsigned long long>(row.committed));
    }
    std::puts("]}");
}
} // namespace
int main(int argc, char **argv) {
    if (argc != 2)
        return 2;
    const std::string_view argument{argv[1]};
    std::uint32_t pid{};
    const auto parsed = std::from_chars(argument.data(), argument.data() + argument.size(), pid);
    if (parsed.ec != std::errc{} || parsed.ptr != argument.data() + argument.size() || !pid ||
        pid > 2147483647U)
        return 2;
    Query query;
    const auto status = query.open(pid);
    if (status != ERROR_SUCCESS) {
        error(status, "open");
        return 1;
    }
    std::puts("{\"schema\":1,\"ready\":true}");
    std::fflush(stdout);
    std::array<char, 16> command{};
    while (std::fgets(command.data(), static_cast<int>(command.size()), stdin)) {
        std::string_view text{command.data()};
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
            text.remove_suffix(1);
        if (text == "quit")
            return 0;
        if (text != "read")
            return 2;
        read(query, pid);
        std::fflush(stdout);
    }
    return std::ferror(stdin) ? 1 : 0;
}
