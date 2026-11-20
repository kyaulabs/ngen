#include "assets/package.hpp"
#include "simulation/simulation.hpp"
#include <array>
#include <charconv>
#include <exception>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>
#include <system_error>

namespace {
struct PlayedBoard {
    ngen::assets::PackagePin package;
    ngen::Snapshot simulation;
    std::uint64_t generation{}, revision{};
};

void fail(std::string_view code) {
    std::cout << "{\"ok\":false,\"error\":\"" << code << "\"}\n" << std::flush;
}

void inspect(const PlayedBoard &board) {
    if (!board.package) {
        fail("inactive");
        return;
    }
    std::cout << "{\"ok\":true,\"generation\":" << board.generation
              << ",\"revision\":" << board.revision << ",\"tick\":" << board.simulation.tick().value
              << ",\"marker_mm\":" << board.simulation.poses().front().position.value
              << ",\"instances\":" << board.package->instances.size() << "}\n"
              << std::flush;
}

template <class Integer> bool field(std::string_view &text, Integer &value, bool last = false) {
    const auto end = text.find(' ');
    if ((end == std::string_view::npos) != last)
        return false;
    const auto token = text.substr(0, end);
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size())
        return false;
    text.remove_prefix(last ? token.size() : end + 1);
    return true;
}

void load(PlayedBoard &board, std::string_view arguments) {
    std::uint64_t expected = 0, revision = 0;
    std::int64_t anchor = 0;
    if (!field(arguments, expected) || !field(arguments, revision) ||
        !field(arguments, anchor, true) ||
        revision > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
        anchor < -8'192'000 || anchor > 8'192'000) {
        fail("command");
        return;
    }
    if (expected != board.generation || revision <= board.revision) {
        fail("stale");
        return;
    }
    // The parent stages a revision in the session directory. Commands cannot
    // select a pathname or access the authoring workspace.
    auto loaded = ngen::assets::load_package_file("r" + std::to_string(revision) + ".ngas");
    const auto *package = std::get_if<ngen::assets::PackagePin>(&loaded);
    if (!package || (*package)->instances.empty()) {
        fail("package");
        return;
    }
    ngen::Simulation simulation;
    const auto created = simulation.create();
    const auto *entity = std::get_if<ngen::Entity>(&created);
    if (!entity || !std::holds_alternative<std::monostate>(
                       simulation.submit({ngen::Tick{1}, 1, *entity, {anchor + 42}}))) {
        fail("simulation");
        return;
    }
    const auto advanced = simulation.step();
    const auto *snapshot = std::get_if<ngen::Snapshot>(&advanced);
    if (!snapshot || snapshot->poses().size() != 1) {
        fail("simulation");
        return;
    }
    board = {*package, *snapshot, board.generation + 1, revision};
    inspect(board);
}
} // namespace

int main() try {
    PlayedBoard board;
    std::array<char, 128> buffer{};
    while (std::cin.getline(buffer.data(), static_cast<std::streamsize>(buffer.size()))) {
        if (std::cin.eof())
            break; // A final incomplete line cannot mutate the active snapshot.
        std::string_view command(buffer.data(), static_cast<std::size_t>(std::cin.gcount() - 1));
        if (command == "inspect")
            inspect(board);
        else if (command == "stop") {
            std::cout << "{\"ok\":true,\"stopped\":true}\n";
            return 0;
        } else if (command.starts_with("load "))
            load(board, command.substr(5));
        else
            fail("command");
    }
    return std::cin.eof() ? 0 : 2;
} catch (const std::exception &) {
    fail("worker");
    return 1;
}
