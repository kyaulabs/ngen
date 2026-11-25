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
    if (journey_)
        return VoyageFrame{snapshot().vessel, journey_->snapshot()};
    return {};
}

Result<travel::Route> Voyage::route() const {
    const std::size_t source = at_starter(snapshot().stage) ? 0 : 1;
    return catalog_.gate(catalog_.systems[source].id, catalog_.systems[1 - source].id, true);
}

Result<VoyageFrame> Voyage::prepare_local(const travel::CollisionSite &site) {
    if (journey_ || snapshot().stage == Stage::lost)
        return Error::busy;
    const auto expected = departure();
    if (site.origin != expected.origin || site.revision != expected.revision)
        return Error::stale;
    const auto planned = route();
    if (const auto *error = std::get_if<Error>(&planned))
        return *error;
    auto result =
        travel::Journey::create(std::get<travel::Route>(planned), ship_, site.obstacles, tick_);
    if (const auto *error = std::get_if<Error>(&result))
        return *error;
    journey_ = std::get<std::unique_ptr<travel::Journey>>(std::move(result));
    return VoyageFrame{snapshot().vessel, journey_->snapshot()};
}

Result<std::monostate> Voyage::submit(flight::Intent intent) {
    if (!journey_ || active_)
        return Error::busy;
    const auto admission = envelope();
    const auto upcoming = next(admission.expected);
    if (const auto *error = std::get_if<Error>(&upcoming))
        return *error;
    const auto submitted =
        journey_->submit({std::get<Tick>(upcoming), admission.sequence, admission.vessel, intent});
    if (std::holds_alternative<std::monostate>(submitted))
        ++sequence_;
    return submitted;
}

VoyageOutcome Voyage::request(Command command, const travel::CollisionSite *site) {
    if (active_)
        return Error::busy;
    const auto prior = revision();
    if (command.expected != prior || !travels(command.action)) {
        const auto result = save_->step(command);
        if (revision() != prior &&
            (command.action == Action::lose_ship || command.action == Action::recover))
            journey_.reset();
        return compose(result);
    }
    const auto proposal = save_->preview(command);
    if (const auto *rejection = std::get_if<Rejection>(&proposal))
        return *rejection;

    const auto expected = departure();
    if (site && (site->origin != expected.origin || site->revision != expected.revision))
        return Error::stale;
    if (!journey_ && !site)
        return Error::busy;
    const auto planned = route();
    if (const auto *error = std::get_if<Error>(&planned))
        return *error;
    const auto &path = std::get<travel::Route>(planned);
    std::unique_ptr<travel::Journey> prepared;
    auto *local = journey_.get();
    if (!local) {
        auto created = travel::Journey::create(path, ship_, site->obstacles, tick_);
        if (const auto *error = std::get_if<Error>(&created))
            return *error;
        prepared = std::get<std::unique_ptr<travel::Journey>>(std::move(created));
        local = prepared.get();
    }
    const auto start = local->snapshot();
    const auto armed = local->begin_from_berth({start.vessel, start.tick, sequence_ + 1}, path);
    if (const auto *error = std::get_if<Error>(&armed))
        return *error;
    ++sequence_;
    if (prepared)
        journey_ = std::move(prepared);
    active_ = Active{command, {path.destination, path.collision_revision}};
    return InTransit{};
}

Result<VoyageFrame> Voyage::advance() {
    if (!journey_)
        return Error::busy;
    const auto result = journey_->step();
    if (const auto *error = std::get_if<Error>(&result))
        return *error;
    const auto &state = std::get<travel::Snapshot>(result);
    tick_ = state.tick;
    return VoyageFrame{snapshot().vessel, state};
}

travel::Command Voyage::envelope() const {
    const auto state = journey_->snapshot();
    return {state.vessel, state.tick, sequence_ + 1};
}

Result<std::monostate> Voyage::cancel() {
    if (!active_)
        return Error::busy;
    const auto result = journey_->cancel(envelope(), journey_->snapshot().trip);
    if (const auto *error = std::get_if<Error>(&result))
        return *error;
    ++sequence_;
    active_.reset();
    return std::monostate{};
}

VoyageOutcome Voyage::arrive(const travel::CollisionSite *site) {
    if (!active_)
        return Error::busy;
    const auto arrival = journey_->arrive(envelope(), journey_->snapshot().trip, site);
    if (const auto *error = std::get_if<Error>(&arrival))
        return *error;
    ++sequence_;

    const auto command = active_->command;
    const auto result = save_->step(command);
    // A post-rename sync error can report uncertainty after advancing the
    // published revision. Keep that arrival and let an exact retry resync.
    if (revision() == command.expected)
        journey_.reset(); // Do not expose an arrival that did not publish.
    active_.reset();
    return compose(result);
}
} // namespace ngen::quests
