#if defined(NGEN_SURVEY_VOYAGE)
#include "quests/voyage.hpp"
#else
#include "quests/save.hpp"
#endif
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <exception>
#include <iostream>
#include <optional>
#include <string_view>

using namespace ngen::quests;

namespace {
constexpr std::array<std::string_view, 9> commands{"learn", "join",    "venture", "mine", "defeat",
                                                   "lose",  "recover", "salvage", "sell"};
static_assert(commands.size() == static_cast<std::size_t>(Action::dock_and_sell) + 1);

std::string_view describe(Stage stage) {
    switch (stage) {
    case Stage::training:
        return "Protected space: lawful self-defense is permitted. An unprovoked strike can bring "
               "police intervention and fines. This is a lesson, not live police enforcement. "
               "Next: learn.";
    case Stage::enlistment:
        return "Choose the Surveyors' prospecting career. First completed survey pays 4 credits. "
               "Next: join.";
    case Stage::protected_port:
        return "At the protected port. The lawless route has no NPC police and costs 2 credits per "
               "trip. "
               "Accept the risk: venture.";
    case Stage::prospecting:
        return "Lawless belt: extract up to 3 units for this sortie. Next: mine.";
    case Stage::encounter:
        return "Lawless rival encounter. Choose the audited victory or loss route: defeat / lose.";
    case Stage::lost:
        return "Your wreck owns all surviving ore. A free starter grants no cargo or credits. "
               "Next: recover.";
    case Stage::recovered:
        return "The starter hold is empty. Claim the wreck to recover its existing ore. Next: "
               "salvage.";
    case Stage::returning:
        return "Return to the protected station, capture the berth and sell for 5 credits per "
               "unit. Next: sell.";
    }
    return {};
}

std::string_view code(Rejection error) {
    switch (error) {
    case Rejection::wrong_stage:
        return "wrong_stage";
    case Rejection::order:
        return "order";
    case Rejection::conflict:
        return "conflict";
    case Rejection::exhausted:
        return "exhausted";
    case Rejection::fixture_failure:
        return "fixture_failure";
    }
    return "rejected";
}

std::string_view code(SaveError error) {
    switch (error) {
    case SaveError::io:
        return "save_io";
    case SaveError::busy:
        return "save_busy";
    case SaveError::invalid:
        return "invalid_save";
    case SaveError::uncertain:
        return "save_uncertain (retry the same revision and action)";
    }
    return "save_error";
}

int reject(std::string_view reason) {
    std::cerr << reason << '\n';
    return 1;
}

struct Input {
    std::string_view revision, action;
};

std::optional<Command> parse(Input input) {
    std::uint32_t expected{};
    const auto end = input.revision.end();
    const auto number = std::from_chars(input.revision.begin(), end, expected);
    const auto match = std::ranges::find(commands, input.action);
    if (number.ec != std::errc{} || number.ptr != end || match == commands.end())
        return {};
    return Command{expected, static_cast<Action>(match - commands.begin())};
}

#if defined(NGEN_SURVEY_VOYAGE)
using Campaign = Voyage;
constexpr auto usage = "usage: quest_voyage PRIVATE_DIRECTORY status | step REVISION ACTION\n";

std::string_view label(ngen::travel::Security security) {
    return security == ngen::travel::Security::protected_space ? "protected" : "lawless";
}

auto open(const std::filesystem::path &directory) { return Voyage::open(directory, {}); }

VoyageOutcome apply(Campaign &campaign, Command command) {
    const auto from = campaign.departure();
    // The CLI owns two fixed empty contact profiles; a graphical producer
    // must pass its prepared source and destination instead.
    const ngen::travel::CollisionSite source{from.origin, from.revision, {}};
    const auto result = campaign.request(command, &source);
    if (!std::holds_alternative<InTransit>(result))
        return result;
    const auto target = campaign.destination();
    if (!target)
        return ngen::Error::order;
    for (unsigned tick = 0; tick < 16; ++tick) {
        const auto crossed = campaign.advance();
        if (const auto *error = std::get_if<ngen::Error>(&crossed))
            return *error;
        if (std::get<VoyageFrame>(crossed).journey.phase != ngen::travel::Phase::exit)
            continue;
        const ngen::travel::CollisionSite site{target->origin, target->revision, {}};
        const auto arrival = campaign.arrive(&site);
        if (std::holds_alternative<Receipt>(arrival))
            std::cout << "native_arrival=" << label(campaign.security()) << '\n';
        return arrival;
    }
    return ngen::Error::order;
}
#else
using Campaign = Save;
constexpr auto usage = "usage: quest_survey PRIVATE_DIRECTORY status | step REVISION ACTION\n";

auto open(const std::filesystem::path &directory) { return Save::open(directory); }

SavedOutcome apply(Campaign &campaign, Command command) { return campaign.step(command); }
#endif

void show(const Campaign &save) {
    const auto state = save.snapshot();
    std::cout << "revision=" << save.revision() << " vessel=" << state.vessel
              << " credits=" << state.credits << " cargo=" << state.cargo
              << " wreck=" << state.wreck << " deposit=" << state.deposit
              << " completed=" << state.completed_runs << '\n';
#if defined(NGEN_SURVEY_VOYAGE)
    std::cout << "committed_system=" << label(save.security()) << '\n';
#endif
    if (!state.deposit && state.stage == Stage::protected_port)
        std::cout << "Survey complete: the bounded deposit is exhausted.\n";
    else
        std::cout << describe(state.stage) << '\n';
}
} // namespace

int main(int argc, char **argv) try {
    if (!((argc == 3 && std::string_view(argv[2]) == "status") ||
          (argc == 5 && std::string_view(argv[2]) == "step"))) {
        std::cerr << usage;
        return 2;
    }
    const auto command = argc == 5 ? parse({argv[3], argv[4]}) : std::nullopt;
    if (argc == 5 && !command)
        return reject("invalid_command");
    auto opened = open(argv[1]);
    if (const auto *error = std::get_if<SaveError>(&opened))
        return reject(code(*error));
    auto &save = *std::get<std::unique_ptr<Campaign>>(opened);
    if (command) {
        const auto outcome = apply(save, *command);
#if defined(NGEN_SURVEY_VOYAGE)
        if (const auto *error = std::get_if<ngen::Error>(&outcome)) {
            std::cerr << "flight_error=" << static_cast<unsigned>(*error) << '\n';
            return 1;
        }
#endif
        if (const auto *error = std::get_if<SaveError>(&outcome))
            return reject(code(*error));
        if (const auto *rejection = std::get_if<Rejection>(&outcome))
            return reject(code(*rejection));
        std::cout << "receipt=" << std::get<Receipt>(outcome).revision << '\n';
    }
    show(save);
    return 0;
} catch (const std::exception &) {
    std::fputs("Surveyors: unexpected host error\n", stderr);
    return 1;
}
