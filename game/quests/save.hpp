#pragma once
#include "quests/survey.hpp"
#include <filesystem>
#include <memory>

namespace ngen::quests {
enum class SaveError : std::uint8_t { io, busy, invalid, uncertain };
using SavedOutcome = std::variant<Receipt, Rejection, SaveError>;

// Linux offline store for one private owner directory. Opening holds the save
// lock until destruction. Never expose the working campaign before publication.
class Save {
  public:
    [[nodiscard]] static std::variant<std::unique_ptr<Save>, SaveError>
    open(const std::filesystem::path &directory);
    ~Save();
    Save(const Save &) = delete;
    Save &operator=(const Save &) = delete;

    [[nodiscard]] State snapshot() const { return survey_.snapshot(); }
    [[nodiscard]] std::uint32_t revision() const { return survey_.revision(); }
    [[nodiscard]] SavedOutcome step(Command command);

  private:
    friend class Voyage;
    [[nodiscard]] static std::variant<std::unique_ptr<Save>, SaveError>
    open_slot(const std::filesystem::path &directory, bool flight_slot);
    [[nodiscard]] Outcome preview(Command command) const;
    Save(int directory, bool flight_slot) : directory_(directory), flight_slot_(flight_slot) {}
    const int directory_;
    const bool flight_slot_;
    Survey survey_;
};
} // namespace ngen::quests
