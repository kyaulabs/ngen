#include "quests/voyage.hpp"
#include <utility>

namespace ngen::quests {
namespace {
VoyageOutcome compose(SavedOutcome saved) {
    return std::visit([](auto result) -> VoyageOutcome { return result; }, saved);
}

bool at_starter(Stage stage) {
    return stage == Stage::training || stage == Stage::enlistment || stage == Stage::protected_port;
}

bool travels(Action action) {
    return action == Action::accept_risk || action == Action::dock_and_sell;
}
} // namespace

Voyage::Voyage(std::unique_ptr<Save> save, const flight::ShipDefinition &ship)
    : save_(std::move(save)), ship_(ship) {}

std::variant<std::unique_ptr<Voyage>, SaveError>
Voyage::open(const std::filesystem::path &directory, const flight::ShipDefinition &ship) {
    auto result = Save::open_slot(directory, true);
    if (const auto *error = std::get_if<SaveError>(&result))
        return *error;
    return std::unique_ptr<Voyage>(
        new Voyage(std::get<std::unique_ptr<Save>>(std::move(result)), ship));
}

travel::Security Voyage::security() const {
    return catalog_.systems[at_starter(snapshot().stage) ? 0 : 1].security;
}

SiteTarget Voyage::departure() const {
    const auto &system = catalog_.systems[at_starter(snapshot().stage) ? 0 : 1];
    return {system.berth, system.collision_revision};
}

std::optional<SiteTarget> Voyage::destination() const {
    if (active_)
        return active_->target;
    return {};
}

std::optional<VoyageFrame> Voyage::native() const {
    if (active_)
        return VoyageFrame{snapshot().vessel, active_->journey->snapshot()};
    if (parked_)
        return VoyageFrame{snapshot().vessel, parked_->snapshot()};
    return {};
}

VoyageOutcome Voyage::request(Command command, const travel::CollisionSite *site) {
    if (active_)
        return Error::busy;
    const auto prior = revision();
    if (command.expected != prior || !travels(command.action)) {
        const auto result = save_->step(command);
        if (revision() != prior &&
            (command.action == Action::lose_ship || command.action == Action::recover))
            parked_.reset();
        return compose(result);
    }
    const auto proposal = save_->preview(command);
    if (const auto *rejection = std::get_if<Rejection>(&proposal))
        return *rejection;

    const auto expected = departure();
    if (!site)
        return Error::busy;
    if (site->origin != expected.origin || site->revision != expected.revision)
        return Error::stale;
    const std::size_t source = at_starter(snapshot().stage) ? 0 : 1;
    const auto route =
        catalog_.gate(catalog_.systems[source].id, catalog_.systems[1 - source].id, true);
    if (const auto *error = std::get_if<Error>(&route))
        return *error;
    const auto &path = std::get<travel::Route>(route);
    auto prepared = travel::Journey::create(path, ship_, site->obstacles, tick_);
    if (const auto *error = std::get_if<Error>(&prepared))
        return *error;
    auto journey = std::get<std::unique_ptr<travel::Journey>>(std::move(prepared));
    const auto start = journey->snapshot();
    const auto armed = journey->begin({start.vessel, start.tick, 1});
    if (const auto *error = std::get_if<Error>(&armed))
        return *error;
    active_ = Active{command, {path.destination, path.collision_revision}, std::move(journey)};
    return InTransit{};
}

Result<VoyageFrame> Voyage::advance() {
    auto *journey = active_ ? active_->journey.get() : parked_.get();
    if (!journey)
        return Error::busy;
    const auto result = journey->step();
    if (const auto *error = std::get_if<Error>(&result))
        return *error;
    const auto &state = std::get<travel::Snapshot>(result);
    tick_ = state.tick;
    return VoyageFrame{snapshot().vessel, state};
}

Result<std::monostate> Voyage::cancel() {
    if (!active_)
        return Error::busy;
    const auto state = active_->journey->snapshot();
    const auto result = active_->journey->cancel({state.vessel, state.tick, 2}, state.trip);
    if (const auto *error = std::get_if<Error>(&result))
        return *error;
    active_.reset();
    return std::monostate{};
}

VoyageOutcome Voyage::arrive(const travel::CollisionSite *site) {
    if (!active_)
        return Error::busy;
    const auto state = active_->journey->snapshot();
    const auto arrival = active_->journey->arrive({state.vessel, state.tick, 2}, state.trip, site);
    if (const auto *error = std::get_if<Error>(&arrival))
        return *error;

    const auto command = active_->command;
    const auto result = save_->step(command);
    // A post-rename sync error can report uncertainty after advancing the
    // published revision. Keep that arrival and let an exact retry resync.
    if (revision() != command.expected)
        parked_ = std::move(active_->journey);
    else
        parked_.reset(); // Do not expose the older native tick after elapsed transit.
    active_.reset();
    return compose(result);
}
} // namespace ngen::quests
