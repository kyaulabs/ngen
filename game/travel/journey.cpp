#include "travel/journey.hpp"
#include <cmath>
#include <new>
#include <utility>

namespace {
bool parked(const ngen::flight::Snapshot &local) {
    const auto &body = local.body;
    // The scoped route starts at the authored berth. No arbitrary attitude,
    // momentum or active steering is discarded to enter the trajectory.
    return local.mode == ngen::flight::Mode::tactical &&
           local.navigation.goal == ngen::flight::Goal::coast &&
           body.position_metres == std::array<double, 3>{} &&
           body.rotation_xyzw == std::array<float, 4>{0, 0, 0, 1} &&
           body.velocity_mps == std::array<float, 3>{} &&
           body.angular_velocity_rps == std::array<float, 3>{} &&
           local.force_newtons == std::array<float, 3>{} &&
           local.torque_newton_metres == std::array<float, 3>{};
}
// Only stationary axis-aligned proxy boxes are admitted by Flight. Test the
// authored arrival berth before constructing or publishing the destination.
ngen::Result<std::monostate> clear_berth(const ngen::physics::Box &hull,
                                         std::span<const ngen::physics::Box> obstacles) {
    if (obstacles.size() >= ngen::physics::World::body_capacity)
        return ngen::Error::capacity;
    for (const auto &box : obstacles) {
        bool separated = false;
        for (std::size_t i = 0; i < hull.half_extent_metres.size(); ++i) {
            const auto reach =
                static_cast<double>(hull.half_extent_metres[i]) + box.half_extent_metres[i] + .05;
            separated |= std::abs(box.position_metres[i]) > reach;
        }
        if (!separated)
            return ngen::Error::busy;
    }
    return std::monostate{};
}
} // namespace

namespace ngen::travel {
Journey::Journey(Route route, flight::ShipDefinition ship, Trajectory path,
                 std::unique_ptr<flight::Flight> local)
    : route_(route), ship_(ship), path_(path), flight_(std::move(local)) {
    const auto initial = flight_->snapshot();
    state_ = {Phase::local, initial.tick, initial.entity, 0, route_.source, initial};
}

Result<std::unique_ptr<Journey>> Journey::create(const Route &route,
                                                 const flight::ShipDefinition &ship,
                                                 std::span<const physics::Box> obstacles) {
    if (!route.collision_revision || ship.hull.position_metres != std::array<double, 3>{})
        return Error::invalid;
    const auto clear = clear_berth(ship.hull, obstacles);
    if (const auto *error = std::get_if<Error>(&clear))
        return *error;
    const auto path = Trajectory::create(route.source, route.destination, route.duration_ticks);
    if (const auto *error = std::get_if<Error>(&path))
        return *error;
    try {
        auto local = flight::Flight::create(ship, obstacles, route.source);
        if (const auto *error = std::get_if<Error>(&local))
            return *error;
        return std::unique_ptr<Journey>(
            new Journey(route, ship, std::get<Trajectory>(path),
                        std::get<std::unique_ptr<flight::Flight>>(std::move(local))));
    } catch (const std::bad_alloc &) {
        return Error::capacity;
    }
}

Snapshot Journey::snapshot() const { return state_; }

Result<std::monostate> Journey::check(const Command &command) const {
    if (owner_ != std::this_thread::get_id())
        return Error::order;
    if (command.vessel != state_.vessel)
        return Error::stale;
    if (command.expected != state_.tick || command.sequence <= sequence_)
        return Error::order;
    return std::monostate{};
}

Result<Snapshot> Journey::begin(const Command &command) {
    const auto checked = check(command);
    if (const auto *error = std::get_if<Error>(&checked))
        return *error;
    if (state_.phase != Phase::local || pending_)
        return Error::busy;
    const auto local = flight_->snapshot();
    if (!route_.jump_capable || !route_.route_allowed || !route_.security_allowed || !parked(local))
        return Error::invalid;
    ship_.hull.mass_kg = local.body.mass_kg;
    sequence_ = command.sequence;
    state_.phase = Phase::entry;
    state_.trip = command.sequence;
    return state_;
}

Result<Snapshot> Journey::cancel(const Command &command, std::uint64_t trip) {
    const auto checked = check(command);
    if (const auto *error = std::get_if<Error>(&checked))
        return *error;
    if (state_.phase != Phase::entry)
        return Error::busy;
    if (trip != state_.trip)
        return Error::stale;
    sequence_ = command.sequence;
    state_.phase = Phase::local;
    state_.trip = 0;
    return state_;
}

Result<std::monostate> Journey::submit(const flight::Command &command) {
    if (owner_ != std::this_thread::get_id())
        return Error::order;
    if (state_.phase != Phase::local && state_.phase != Phase::arrived)
        return Error::busy;
    if (command.sequence <= sequence_)
        return Error::order;
    const auto submitted = flight_->submit(command);
    if (const auto *error = std::get_if<Error>(&submitted))
        return *error;
    pending_ = true;
    sequence_ = command.sequence;
    return std::monostate{};
}

Result<Snapshot> Journey::step() {
    if (owner_ != std::this_thread::get_id())
        return Error::order;
    if (state_.phase == Phase::local || state_.phase == Phase::arrived) {
        const auto advanced = flight_->step();
        if (const auto *error = std::get_if<Error>(&advanced)) {
            pending_ = true;
            return *error;
        }
        const auto &local = std::get<flight::Snapshot>(advanced);
        state_.local = local;
        state_.tick = local.tick;
        pending_ = false;
        return state_;
    }
    const auto upcoming = next(state_.tick);
    if (const auto *error = std::get_if<Error>(&upcoming))
        return *error;
    if (state_.phase == Phase::entry) {
        flight_.reset();
        state_.local.reset();
        state_.phase = Phase::transit;
    } else if (state_.phase == Phase::transit) {
        const auto sample = path_.sample(elapsed_ + 1);
        if (const auto *error = std::get_if<Error>(&sample))
            return *error;
        state_.route_position = std::get<coordinates::Position>(sample);
        if (++elapsed_ == route_.duration_ticks)
            state_.phase = Phase::exit;
    }
    state_.tick = std::get<Tick>(upcoming);
    return state_;
}

Result<Snapshot> Journey::arrive(const Command &command, std::uint64_t trip,
                                 const CollisionSite *site) {
    const auto checked = check(command);
    if (const auto *error = std::get_if<Error>(&checked))
        return *error;
    if (state_.phase != Phase::exit || !site)
        return Error::busy;
    if (trip != state_.trip || site->revision != route_.collision_revision ||
        site->origin != route_.destination)
        return Error::stale;
    const auto clear = clear_berth(ship_.hull, site->obstacles);
    if (const auto *error = std::get_if<Error>(&clear))
        return *error;
    try {
        auto prepared = flight::Flight::create(ship_, site->obstacles, site->origin, state_.tick);
        if (const auto *error = std::get_if<Error>(&prepared))
            return *error;
        flight_ = std::get<std::unique_ptr<flight::Flight>>(std::move(prepared));
    } catch (const std::bad_alloc &) {
        return Error::capacity;
    }
    state_.local = flight_->snapshot();
    state_.phase = Phase::arrived;
    sequence_ = command.sequence;
    return state_;
}
} // namespace ngen::travel
