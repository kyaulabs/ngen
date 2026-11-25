#include "quests/save.hpp"
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

void show(const Save &save) {
    const auto state = save.snapshot();
    std::cout << "revision=" << save.revision() << " vessel=" << state.vessel
              << " credits=" << state.credits << " cargo=" << state.cargo
              << " wreck=" << state.wreck << " deposit=" << state.deposit
              << " completed=" << state.completed_runs << '\n';
    if (!state.deposit && state.stage == Stage::protected_port)
        std::cout << "Survey complete: the bounded deposit is exhausted.\n";
    else
        std::cout << describe(state.stage) << '\n';
}
} // namespace

int main(int argc, char **argv) try {
    if (!((argc == 3 && std::string_view(argv[2]) == "status") ||
          (argc == 5 && std::string_view(argv[2]) == "step"))) {
        std::cerr << "usage: quest_survey PRIVATE_DIRECTORY status | step REVISION ACTION\n";
        return 2;
    }
    const auto command = argc == 5 ? parse({argv[3], argv[4]}) : std::nullopt;
    if (argc == 5 && !command)
        return reject("invalid_command");
    auto opened = Save::open(argv[1]);
    if (const auto *error = std::get_if<SaveError>(&opened))
        return reject(code(*error));
    auto &save = *std::get<std::unique_ptr<Save>>(opened);
    if (command) {
        const auto outcome = save.step(*command);
        if (const auto *error = std::get_if<SaveError>(&outcome))
            return reject(code(*error));
        if (const auto *rejection = std::get_if<Rejection>(&outcome))
            return reject(code(*rejection));
        std::cout << "receipt=" << std::get<Receipt>(outcome).revision << '\n';
    }
    show(save);
    return 0;
} catch (const std::exception &) {
    std::fputs("quest_survey: unexpected host error\n", stderr);
    return 1;
}
