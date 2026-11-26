#include "quests/save.hpp"
#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <fcntl.h>
#include <optional>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#include <utility>

namespace ngen::quests {
namespace {
struct Slot {
    const char *committed;
    const char *staged;
    // Slot magic, save format version, campaign/rule version.
    std::array<std::uint8_t, 6> header;
};
constexpr Slot headless{"ngen-survey.save", ".ngen-survey.pending", {'N', 'Q', 'S', 'V', 1, 1}};
constexpr Slot flight{"ngen-voyage.save", ".ngen-voyage.pending", {'N', 'Q', 'S', 'F', 1, 1}};
constexpr std::size_t checksum_bytes = 4;
using Buffer = std::array<std::uint8_t, headless.header.size() + Survey::capacity + checksum_bytes>;

class Descriptor {
  public:
    explicit Descriptor(int fd) : fd_(fd) {}
    ~Descriptor() {
        if (fd_ >= 0)
            close(fd_);
    }
    Descriptor(const Descriptor &) = delete;
    Descriptor &operator=(const Descriptor &) = delete;
    [[nodiscard]] int get() const { return fd_; }
    [[nodiscard]] int release() { return std::exchange(fd_, -1); }

  private:
    int fd_;
};

std::array<std::uint8_t, checksum_bytes> checksum(std::span<const std::uint8_t> data) {
    std::uint32_t hash = 2166136261U;
    for (const auto byte : data) {
        hash ^= byte;
        hash *= 16777619U;
    }
    return {static_cast<std::uint8_t>(hash >> 24U), static_cast<std::uint8_t>(hash >> 16U),
            static_cast<std::uint8_t>(hash >> 8U), static_cast<std::uint8_t>(hash)};
}

bool write_all(int fd, std::span<const std::uint8_t> data) {
    while (!data.empty()) {
        const auto count = write(fd, data.data(), data.size());
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return false;
        data = data.subspan(static_cast<std::size_t>(count));
    }
    return true;
}

std::optional<SaveError> load(int directory, const Slot &slot, Survey &survey) {
    const auto &header = slot.header;
    const int fd =
        openat(directory, slot.committed, O_RDONLY | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK);
    if (fd < 0) {
        if (errno == ENOENT)
            return {};
        return errno == ELOOP ? SaveError::invalid : SaveError::io;
    }
    const Descriptor file(fd);
    struct stat identity = {};
    if (fstat(file.get(), &identity) != 0)
        return SaveError::io;
    if (!S_ISREG(identity.st_mode) || identity.st_uid != geteuid() || identity.st_nlink != 1)
        return SaveError::invalid;
    Buffer bytes{};
    if (identity.st_size < static_cast<off_t>(header.size() + checksum_bytes) ||
        identity.st_size > static_cast<off_t>(bytes.size()))
        return SaveError::invalid;
    const auto size = static_cast<std::size_t>(identity.st_size);
    std::size_t offset = 0;
    while (offset < size) {
        const auto count = read(file.get(), bytes.data() + offset, size - offset);
        if (count < 0 && errno == EINTR)
            continue;
        if (count <= 0)
            return count == 0 ? SaveError::invalid : SaveError::io;
        offset += static_cast<std::size_t>(count);
    }
    const auto content = std::span{bytes}.first(size - checksum_bytes);
    if (!std::ranges::equal(content.first(header.size()), header) ||
        !std::ranges::equal(std::span{bytes}.subspan(content.size(), checksum_bytes),
                            checksum(content)))
        return SaveError::invalid;
    for (const auto action : content.subspan(header.size())) {
        if (action > static_cast<std::uint8_t>(Action::dock_and_sell) ||
            !std::holds_alternative<Receipt>(
                survey.step({survey.revision(), static_cast<Action>(action)})))
            return SaveError::invalid;
    }
    return {};
}

bool publish(int directory, const Slot &slot, std::span<const Action> actions) {
    const auto &header = slot.header;
    const auto *staged = slot.staged;
    Buffer bytes{};
    std::ranges::copy(header, bytes.begin());
    const auto content = std::span{bytes}.first(header.size() + actions.size());
    std::ranges::transform(actions, content.subspan(header.size()).begin(),
                           [](Action action) { return static_cast<std::uint8_t>(action); });
    std::ranges::copy(checksum(content), std::span{bytes}.subspan(content.size()).begin());
    // The private workspace and held flock reserve this one staging name.
    // A crash here can leave staged bytes, which load never treats as committed.
    if (unlinkat(directory, staged, 0) != 0 && errno != ENOENT)
        return false;
    const Descriptor file(openat(directory, staged, O_CREAT | O_EXCL | O_WRONLY | O_CLOEXEC, 0600));
    if (file.get() < 0)
        return false;
    if (fchmod(file.get(), 0600) == 0 &&
        write_all(file.get(), std::span{bytes}.first(content.size() + checksum_bytes)) &&
        fsync(file.get()) == 0 && renameat(directory, staged, directory, slot.committed) == 0)
        return true;
    unlinkat(directory, staged, 0);
    return false;
}
} // namespace

std::variant<std::unique_ptr<Save>, SaveError> Save::open(const std::filesystem::path &path) {
    return open_slot(path, false);
}

std::variant<std::unique_ptr<Save>, SaveError> Save::open_slot(const std::filesystem::path &path,
                                                               bool flight_slot) {
    if (path.native().find('\0') != std::string::npos)
        return SaveError::invalid;
    Descriptor directory(::open(path.c_str(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC));
    if (directory.get() < 0)
        return SaveError::io;
    struct stat identity = {};
    if (fstat(directory.get(), &identity) != 0)
        return SaveError::io;
    if (identity.st_uid != geteuid() || (identity.st_mode & 0777U) != 0700U)
        return SaveError::invalid;
    if (flock(directory.get(), LOCK_EX | LOCK_NB) != 0)
        return errno == EWOULDBLOCK ? SaveError::busy : SaveError::io;
    auto save = std::unique_ptr<Save>(new Save(directory.release(), flight_slot));
    if (const auto error = load(save->directory_, flight_slot ? flight : headless, save->survey_))
        return *error;
    return save;
}

Save::~Save() { close(directory_); }

Outcome Save::preview(Command command) const {
    auto candidate = survey_;
    return candidate.step(command);
}

SavedOutcome Save::step(Command command, SaveTimings *timings) {
    if (timings)
        *timings = {};
    auto candidate = survey_;
    const auto outcome = candidate.step(command, timings ? &timings->market_ns : nullptr);
    if (const auto *rejected = std::get_if<Rejection>(&outcome))
        return *rejected;
    using Clock = std::chrono::steady_clock;
    const auto started = timings ? Clock::now() : Clock::time_point{};
    const auto persisted = [&]() -> SavedOutcome {
        if (candidate.revision() != survey_.revision()) {
            if (!publish(directory_, flight_slot_ ? flight : headless, candidate.history()))
                return SaveError::io;
            survey_ = candidate;
        }
        // A failed directory sync after rename is ambiguous across power loss.
        // Retrying the same revision/action observes old or new without a double payout.
        if (fsync(directory_) != 0)
            return SaveError::uncertain;
        return std::get<Receipt>(outcome);
    }();
    if (timings)
        timings->persistence_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - started).count());
    return persisted;
}
} // namespace ngen::quests
