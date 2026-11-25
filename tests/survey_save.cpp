#include "quests/save.hpp"
#include <array>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <fstream>
#include <sys/stat.h>
#include <utility>

using namespace ngen::quests;
namespace fs = std::filesystem;

namespace {
class Workspace {
  public:
    Workspace() {
        std::array pattern{'/', 't', 'm', 'p', '/', 'n', 'g', 'e', 'n',
                           '-', 'X', 'X', 'X', 'X', 'X', 'X', '\0'};
        const auto *created = mkdtemp(pattern.data());
        REQUIRE(created != nullptr);
        root = created;
    }
    ~Workspace() {
        std::error_code ignored;
        fs::remove_all(root, ignored);
    }
    Workspace(const Workspace &) = delete;
    Workspace &operator=(const Workspace &) = delete;
    fs::path root;
};

class RestrictiveUmask {
  public:
    RestrictiveUmask() : old_(umask(0777U)) {}
    ~RestrictiveUmask() { umask(old_); }
    RestrictiveUmask(const RestrictiveUmask &) = delete;
    RestrictiveUmask &operator=(const RestrictiveUmask &) = delete;

  private:
    const mode_t old_;
};

std::unique_ptr<Save> open(const Workspace &folder) {
    auto opened = Save::open(folder.root);
    REQUIRE(std::holds_alternative<std::unique_ptr<Save>>(opened));
    return std::get<std::unique_ptr<Save>>(std::move(opened));
}

Receipt apply(Save &save, Action action) {
    const auto response = save.step({save.revision(), action});
    REQUIRE(std::holds_alternative<Receipt>(response));
    return std::get<Receipt>(response);
}
} // namespace

TEST_CASE("saved quest reloads every stage and replays each historical outcome", "[survey-save]") {
    Workspace folder;
    constexpr std::array actions{Action::learn_law, Action::join_survey,  Action::accept_risk,
                                 Action::mine,      Action::lose_ship,    Action::recover,
                                 Action::salvage,   Action::dock_and_sell};
    std::array<Receipt, actions.size()> receipts{};
    State state;
    for (std::uint32_t i = 0; i < actions.size(); ++i) {
        auto save = open(folder);
        CHECK(save->snapshot() == state);
        CHECK(save->revision() == i);
        if (i)
            CHECK(save->step({i - 1, actions[i - 1]}) == SavedOutcome{receipts[i - 1]});
        receipts[i] = apply(*save, actions[i]);
        state = receipts[i].after;
    }
    const auto final = open(folder);
    CHECK(final->snapshot() == state);
    CHECK(state.stage == Stage::protected_port);
    CHECK(state.vessel == 2);
    CHECK(state.completed_runs == 1);
    CHECK(state.credits == 27);
    CHECK(state.station_stock == 3);
    for (std::uint32_t i = 0; i < actions.size(); ++i)
        CHECK(final->step({i, actions[i]}) == SavedOutcome{receipts[i]});
    CHECK(final->snapshot() == state);
    CHECK(std::get<SaveError>(Save::open(folder.root)) == SaveError::busy);
}

TEST_CASE("an unpublished sale cannot be loaded or rewarded twice", "[survey-save]") {
    Workspace folder;
    Workspace reference;
    constexpr std::array before_sale{Action::learn_law, Action::join_survey, Action::accept_risk,
                                     Action::mine, Action::defeat_rival};
    for (const auto *target : {&folder, &reference}) {
        auto save = open(*target);
        for (const auto action : before_sale)
            apply(*save, action);
    }
    const auto finished = open(reference);
    const auto sale = apply(*finished, Action::dock_and_sell);
    const auto pending = folder.root / ".ngen-survey.pending";
    fs::copy_file(reference.root / "ngen-survey.save", pending);
    {
        auto save = open(folder);
        CHECK(save->revision() == before_sale.size());
        CHECK(save->snapshot().completed_runs == 0);
        CHECK(save->snapshot().cargo == 3);
        // A staging failure must not change the published bytes or working state.
        REQUIRE(fs::remove(pending));
        REQUIRE(fs::create_directory(pending));
        CHECK(save->step({save->revision(), Action::dock_and_sell}) == SavedOutcome{SaveError::io});
        CHECK(save->revision() == before_sale.size());
        CHECK(save->snapshot().credits == 8);
        REQUIRE(fs::remove(pending));
        CHECK(apply(*save, Action::dock_and_sell) == sale);
    }
    auto save = open(folder);
    CHECK(save->step({sale.revision - 1, Action::dock_and_sell}) == SavedOutcome{sale});
    CHECK(save->snapshot().credits == 27);
    CHECK(save->snapshot().station_stock == 3);
    CHECK(save->snapshot().completed_runs == 1);
}

TEST_CASE("a damaged or nonregular save is rejected instead of resetting progress",
          "[survey-save]") {
    Workspace folder;
    {
        auto save = open(folder);
        apply(*save, Action::learn_law);
    }
    const auto path = folder.root / "ngen-survey.save";
    const auto size = fs::file_size(path);
    REQUIRE(size > 1);
    fs::resize_file(path, size - 1);
    CHECK(std::get<SaveError>(Save::open(folder.root)) == SaveError::invalid);
    CHECK(fs::file_size(path) == size - 1);
    REQUIRE(fs::remove(path));
    fs::create_directory(path);
    CHECK(std::get<SaveError>(Save::open(folder.root)) == SaveError::invalid);
}

TEST_CASE("a restrictive process umask cannot strip the published save owner's read access",
          "[survey-save]") {
    Workspace folder;
    {
        const RestrictiveUmask restrictive;
        auto save = open(folder);
        apply(*save, Action::learn_law);
    }
    const auto permissions = fs::status(folder.root / "ngen-survey.save").permissions();
    CHECK(permissions == (fs::perms::owner_read | fs::perms::owner_write));
    CHECK(open(folder)->revision() == 1);
}
