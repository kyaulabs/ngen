#include <Jolt/Jolt.h>

#include "flight/flight.hpp"
#include <algorithm>
#include <cmath>
#include <thread>
#include <type_traits>

namespace ngen::flight {
namespace {
bool valid(const ShipDefinition &ship) {
    if (ship.rcs) {
        for (const auto &bank : ship.rcs->banks) {
            bool thrust = false;
            for (std::size_t axis = 0; axis < 3; ++axis) {
                if (!std::isfinite(bank.position_metres[axis]) ||
                    std::abs(bank.position_metres[axis]) > 1024 ||
                    !std::isfinite(bank.force_newtons[axis]) ||
                    std::abs(bank.force_newtons[axis]) > 1e9F)
                    return false;
                thrust = thrust || bank.force_newtons[axis] != 0;
            }
            if (!thrust)
                return false;
        }
        for (const auto &row : ship.rcs->mix) {
            if (row[5] != 0)
                return false; // The separate main drive owns forward thrust.
            for (const auto value : row)
                if (!std::isfinite(value) || value < 0 || value > 1)
                    return false;
        }
        for (std::size_t column = 0; column < 12; ++column) {
            if (column == 5)
                continue;
            std::array<double, 6> wrench{};
            for (std::size_t bank = 0; bank < rcs_bank_count; ++bank) {
                const auto &jet = ship.rcs->banks[bank];
                std::array<double, 3> force{};
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    force[axis] =
                        static_cast<double>(jet.force_newtons[axis]) * ship.rcs->mix[bank][column];
                    wrench[axis] += force[axis];
                }
                for (std::size_t axis = 0; axis < 3; ++axis) {
                    const auto b = (axis + 1) % 3, c = (axis + 2) % 3;
                    wrench[axis + 3] += static_cast<double>(jet.position_metres[b]) * force[c] -
                                        static_cast<double>(jet.position_metres[c]) * force[b];
                }
            }
            for (std::size_t axis = 0; axis < 6; ++axis) {
                const auto limit = axis < 3 ? ship.maximum_force_newtons[axis]
                                            : ship.maximum_torque_newton_metres[axis - 3];
                const auto expected =
                    axis == column / 2 ? limit * (column % 2 == 0 ? 1.0F : -1.0F) : 0.0F;
                if (std::abs(wrench[axis] - expected) > static_cast<double>(limit) * 1e-4)
                    return false;
            }
        }
    }
    const auto limits = [](const auto &values, float maximum) {
        return std::all_of(values.begin(), values.end(), [maximum](float v) {
            return std::isfinite(v) && v > 0 && v <= maximum;
        });
    };
    float quaternion_length = 0;
    for (const auto value : ship.flight_camera.rotation_xyzw) {
        if (!std::isfinite(value))
            return false;
        quaternion_length += value * value;
    }
    return ship.hull.dynamic && ship.hull.mass_kg >= 1000 && ship.hull.mass_kg <= 1e9 &&
           limits(ship.maximum_force_newtons, 1e9F) &&
           limits(ship.maximum_torque_newton_metres, 1e12F) &&
           std::isfinite(ship.maximum_speed_mps) && ship.maximum_speed_mps >= .1F &&
           ship.maximum_speed_mps <= 100 && std::isfinite(ship.maximum_angular_speed_rps) &&
           ship.maximum_angular_speed_rps >= .01F && ship.maximum_angular_speed_rps <= 2 &&
           std::all_of(ship.flight_camera.position_metres.begin(),
                       ship.flight_camera.position_metres.end(),
                       [](double v) { return std::isfinite(v) && std::abs(v) <= 1024; }) &&
           std::abs(quaternion_length - 1) < 1e-4F &&
           std::isfinite(ship.flight_camera.vertical_fov_degrees) &&
           ship.flight_camera.vertical_fov_degrees >= 30 &&
           ship.flight_camera.vertical_fov_degrees <= 110;
}
bool valid(const Intent &intent) {
    return std::visit(
        [](const auto &value) {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, Pilot>) {
                const auto axes = [](const auto &values) {
                    return std::all_of(values.begin(), values.end(), [](float v) {
                        return std::isfinite(v) && std::abs(v) <= 1;
                    });
                };
                return axes(value.thrust) && axes(value.rotation);
            } else if constexpr (std::is_same_v<T, SetMode>) {
                return value.mode == Mode::tactical || value.mode == Mode::direct;
            } else if constexpr (std::is_same_v<T, Destination>) {
                return std::all_of(
                    value.position_metres.begin(), value.position_metres.end(),
                    [](double v) { return std::isfinite(v) && std::abs(v) <= 4096; });
            } else if constexpr (std::is_same_v<T, SetMass>) {
                return std::isfinite(value.kilograms) && value.kilograms >= 1000 &&
                       value.kilograms <= 1e9;
            } else if constexpr (std::is_same_v<T, Orbit>) {
                return std::all_of(
                           value.centre_metres.begin(), value.centre_metres.end(),
                           [](double v) { return std::isfinite(v) && std::abs(v) <= 4096; }) &&
                       std::isfinite(value.radius_metres) && value.radius_metres >= 10 &&
                       value.radius_metres <= 2048 && std::isfinite(value.speed_mps) &&
                       value.speed_mps >= .1F && value.speed_mps <= 100;
            } else {
                return false;
            }
        },
        intent);
}
bool feasible(const ShipDefinition &ship, const Navigation &navigation, double mass_kg) {
    if (navigation.goal != Goal::orbit)
        return true;
    const auto &h = ship.hull.half_extent_metres;
    const auto clearance = 2 * std::sqrt(h[0] * h[0] + h[1] * h[1] + h[2] * h[2]) + 1;
    const auto acceleration =
        *std::min_element(ship.maximum_force_newtons.begin(), ship.maximum_force_newtons.end()) /
        mass_kg;
    return navigation.radius_metres >= clearance &&
           navigation.speed_mps <= ship.maximum_speed_mps &&
           navigation.speed_mps * navigation.speed_mps / navigation.radius_metres <=
               acceleration * .5 &&
           navigation.speed_mps / navigation.radius_metres <= ship.maximum_angular_speed_rps * .5F;
}
struct Wrench {
    std::array<float, 3> force{}, torque{};
};
JPH::Vec3 bounded(JPH::Vec3 value, const std::array<float, 3> &maximum) {
    return {std::clamp(value.GetX(), -maximum[0], maximum[0]),
            std::clamp(value.GetY(), -maximum[1], maximum[1]),
            std::clamp(value.GetZ(), -maximum[2], maximum[2])};
}
float speed_scale(const ShipDefinition &ship, const physics::BodyState &body, JPH::Vec3 force) {
    const auto &v = body.velocity_mps;
    const auto divisor = body.mass_kg * physics::World::ticks_per_second;
    const std::array<double, 3> delta{force.GetX() / divisor, force.GetY() / divisor,
                                      force.GetZ() / divisor};
    double speed_squared = 0, a = 0, b = 0;
    for (std::size_t i = 0; i < delta.size(); ++i) {
        speed_squared += static_cast<double>(v[i]) * v[i];
        a += delta[i] * delta[i];
        b += 2 * v[i] * delta[i];
    }
    const auto limit = static_cast<double>(ship.maximum_speed_mps);
    const auto c = speed_squared - std::max(speed_squared, limit * limit);
    if (a > 0 && a + b + c > 0)
        return static_cast<float>(
            std::clamp((-b + std::sqrt(b * b - 4 * a * c)) / (2 * a), 0.0, 1.0));
    return 1;
}
Wrench control_wrench(const ShipDefinition &ship, const physics::BodyState &body,
                      const Pilot &pilot, Mode mode, const Navigation &navigation) {
    const auto &q = body.rotation_xyzw;
    const JPH::Quat rotation{q[0], q[1], q[2], q[3]};
    JPH::Vec3 local{pilot.thrust[0] * ship.maximum_force_newtons[0],
                    pilot.thrust[1] * ship.maximum_force_newtons[1],
                    pilot.thrust[2] * ship.maximum_force_newtons[2]};
    JPH::Vec3 heading = JPH::Vec3::sZero();
    if (mode == Mode::tactical) {
        local = JPH::Vec3::sZero();
        if (navigation.goal != Goal::coast) {
            const auto &p = body.position_metres;
            const auto &target = navigation.target_metres;
            JPH::Vec3 desired{static_cast<float>(target[0] - p[0]) * .5F,
                              static_cast<float>(target[1] - p[1]) * .5F,
                              static_cast<float>(target[2] - p[2]) * .5F};
            JPH::Vec3 feed_forward = JPH::Vec3::sZero();
            if (navigation.goal == Goal::orbit) {
                const JPH::Vec3 horizontal{static_cast<float>(p[0] - target[0]), 0,
                                           static_cast<float>(p[2] - target[2])};
                const auto radius = horizontal.Length();
                const auto radial = radius > .001F ? horizontal / radius : JPH::Vec3::sAxisX();
                auto tangent = JPH::Vec3::sAxisY().Cross(radial);
                if (navigation.clockwise)
                    tangent = -tangent;
                heading = tangent * navigation.speed_mps;
                desired = heading - radial * ((radius - navigation.radius_metres) * .8F) -
                          JPH::Vec3::sAxisY() * static_cast<float>(p[1] - target[1]) * .8F;
                feed_forward = -radial * (navigation.speed_mps * navigation.speed_mps /
                                          std::max(radius, navigation.radius_metres));
            } else if (desired.LengthSq() > 1) {
                heading = desired;
            }
            if (desired.LengthSq() > ship.maximum_speed_mps * ship.maximum_speed_mps)
                desired = desired.Normalized() * ship.maximum_speed_mps;
            const auto &v = body.velocity_mps;
            const auto acceleration = (desired - JPH::Vec3{v[0], v[1], v[2]}) * 2 + feed_forward;
            local = bounded(rotation.Conjugated() * acceleration * static_cast<float>(body.mass_kg),
                            ship.maximum_force_newtons);
        }
    }
    if (pilot.brake) {
        const auto &v = body.velocity_mps;
        const auto local_velocity = rotation.Conjugated() * JPH::Vec3{v[0], v[1], v[2]};
        local = bounded(local_velocity * (-static_cast<float>(body.mass_kg) / .35F),
                        ship.maximum_force_newtons);
    }
    auto force = rotation * local;
    // Scale admitted force, never assign velocity or invent unavailable braking.
    force *= speed_scale(ship, body, force);
    JPH::Vec3 target_rate{pilot.rotation[0] * ship.maximum_angular_speed_rps,
                          pilot.rotation[1] * ship.maximum_angular_speed_rps,
                          pilot.rotation[2] * ship.maximum_angular_speed_rps};
    if (mode == Mode::tactical) {
        target_rate = JPH::Vec3::sZero();
        if (heading.LengthSq() > 1e-8F) {
            const auto goal = rotation.Conjugated() * heading.Normalized();
            auto axis = JPH::Vec3{0, 0, -1}.Cross(goal);
            const auto cosine = std::clamp(-goal.GetZ(), -1.0F, 1.0F);
            if (axis.LengthSq() > 1e-8F)
                axis = axis.Normalized();
            else
                axis = JPH::Vec3::sAxisY();
            target_rate = axis * std::min(ship.maximum_angular_speed_rps, std::acos(cosine) / .35F);
        }
    }
    if (target_rate.LengthSq() > ship.maximum_angular_speed_rps * ship.maximum_angular_speed_rps)
        target_rate = target_rate.Normalized() * ship.maximum_angular_speed_rps;
    const auto &w = body.angular_velocity_rps;
    const auto acceleration =
        (target_rate - rotation.Conjugated() * JPH::Vec3{w[0], w[1], w[2]}) / .1F;
    const auto &h = ship.hull.half_extent_metres;
    const auto mass = static_cast<float>(body.mass_kg);
    const JPH::Vec3 requested_torque{mass * (h[1] * h[1] + h[2] * h[2]) / 3 * acceleration.GetX(),
                                     mass * (h[0] * h[0] + h[2] * h[2]) / 3 * acceleration.GetY(),
                                     mass * (h[0] * h[0] + h[1] * h[1]) / 3 * acceleration.GetZ()};
    const auto torque = rotation * bounded(requested_torque, ship.maximum_torque_newton_metres);
    return {{force.GetX(), force.GetY(), force.GetZ()},
            {torque.GetX(), torque.GetY(), torque.GetZ()}};
}
struct Actuation {
    Wrench wrench;
    RcsLevels firing{};
    float main{}, demand{};
};
Actuation actuate(const ShipDefinition &ship, const physics::BodyState &body,
                  const Wrench &requested, RcsPulses &pulses) {
    const auto &q = body.rotation_xyzw;
    const JPH::Quat rotation{q[0], q[1], q[2], q[3]};
    const auto inverse = rotation.Conjugated();
    const auto force =
        inverse * JPH::Vec3{requested.force[0], requested.force[1], requested.force[2]};
    Actuation result;
    result.main = std::clamp(-force.GetZ() / ship.maximum_force_newtons[2], 0.0F, 1.0F);
    if (!ship.rcs) {
        result.wrench = requested;
        return result;
    }
    const auto demand = std::get<RcsLevels>(rcs_demand(ship, q, requested.force, requested.torque));
    result.demand = *std::max_element(demand.begin(), demand.end());
    const auto torque =
        inverse * JPH::Vec3{requested.torque[0], requested.torque[1], requested.torque[2]};
    const std::array<float, 6> requested_local{force.GetX(),  force.GetY(),  force.GetZ(),
                                               torque.GetX(), torque.GetY(), torque.GetZ()};
    float load = 0;
    for (std::size_t axis = 0; axis < 6; ++axis) {
        if (axis == 2 && requested_local[axis] < 0)
            continue;
        const auto limit = axis < 3 ? ship.maximum_force_newtons[axis]
                                    : ship.maximum_torque_newton_metres[axis - 3];
        load = std::max(load, std::clamp(std::abs(requested_local[axis]) / limit, 0.0F, 1.0F));
    }
    // Valve duty follows requested actuator authority, not arbitrary bank rating
    // units. Full configured force must not become one-third force just because
    // its calibrated bank weights are small compared with a torque maneuver.
    auto gate = demand;
    if (result.demand > 0)
        for (auto &value : gate)
            value = std::min(1.0F, value / result.demand * load);
    const auto gated = std::get<RcsLevels>(pulses.step(gate));
    if (gated != RcsLevels{} && load > 0) {
        const auto duty = std::sqrt(std::sqrt(load));
        for (std::size_t bank = 0; bank < rcs_bank_count; ++bank)
            result.firing[bank] = demand[bank] / duty;
        const auto bank_scale =
            std::max(1.0F, *std::max_element(result.firing.begin(), result.firing.end()));
        for (auto &value : result.firing)
            value /= bank_scale;
    }
    const auto sum = [&] {
        std::array<double, 3> f{0, 0,
                                -static_cast<double>(ship.maximum_force_newtons[2]) * result.main},
            t{};
        for (std::size_t bank = 0; bank < rcs_bank_count; ++bank) {
            const auto &jet = ship.rcs->banks[bank];
            std::array<double, 3> applied{};
            for (std::size_t axis = 0; axis < 3; ++axis) {
                applied[axis] = static_cast<double>(jet.force_newtons[axis]) * result.firing[bank];
                f[axis] += applied[axis];
            }
            for (std::size_t axis = 0; axis < 3; ++axis) {
                const auto b = (axis + 1) % 3, c = (axis + 2) % 3;
                t[axis] += static_cast<double>(jet.position_metres[b]) * applied[c] -
                           static_cast<double>(jet.position_metres[c]) * applied[b];
            }
        }
        return Wrench{
            {static_cast<float>(f[0]), static_cast<float>(f[1]), static_cast<float>(f[2])},
            {static_cast<float>(t[0]), static_cast<float>(t[1]), static_cast<float>(t[2])}};
    };
    result.wrench = sum();
    float limit_scale = 1;
    for (std::size_t axis = 0; axis < 3; ++axis) {
        if (std::abs(result.wrench.force[axis]) > ship.maximum_force_newtons[axis])
            limit_scale = std::min(limit_scale, ship.maximum_force_newtons[axis] /
                                                    std::abs(result.wrench.force[axis]));
        if (std::abs(result.wrench.torque[axis]) > ship.maximum_torque_newton_metres[axis])
            limit_scale = std::min(limit_scale, ship.maximum_torque_newton_metres[axis] /
                                                    std::abs(result.wrench.torque[axis]));
    }
    const auto scale_firings = [&](float scale) {
        for (auto &value : result.firing)
            value *= scale;
        result.main *= scale;
        result.wrench = sum();
    };
    if (limit_scale < 1)
        scale_firings(limit_scale);
    const auto to_world = [&] {
        const auto &f = result.wrench.force, &t = result.wrench.torque;
        const auto world_force = rotation * JPH::Vec3{f[0], f[1], f[2]};
        const auto world_torque = rotation * JPH::Vec3{t[0], t[1], t[2]};
        return Wrench{{world_force.GetX(), world_force.GetY(), world_force.GetZ()},
                      {world_torque.GetX(), world_torque.GetY(), world_torque.GetZ()}};
    };
    auto world = to_world();
    const auto speed =
        speed_scale(ship, body, JPH::Vec3{world.force[0], world.force[1], world.force[2]});
    if (speed < 1) {
        scale_firings(speed);
        world = to_world();
    }
    result.wrench = world;
    return result;
}
} // namespace
struct Flight::Impl {
    std::unique_ptr<physics::World> world;
    ShipDefinition ship;
    physics::BodyId body;
    Snapshot published;
    std::array<Command, command_capacity> commands{};
    std::array<physics::Box, physics::World::body_capacity - 1> obstacles{};
    std::size_t command_count{}, obstacle_count{};
    std::uint64_t sequence{};
    Pilot pilot;
    RcsPulses pulses;
    bool failed{};
};
Flight::Flight(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
Flight::~Flight() = default;
Result<std::unique_ptr<Flight>> Flight::create(const ShipDefinition &definition,
                                               std::span<const physics::Box> static_obstacles) {
    if (static_obstacles.size() >= physics::World::body_capacity)
        return Error::capacity;
    if (!valid(definition) || std::any_of(static_obstacles.begin(), static_obstacles.end(),
                                          [](const auto &box) { return box.dynamic; }))
        return Error::invalid;
    auto created = physics::World::create();
    if (const auto *error = std::get_if<Error>(&created))
        return *error;
    auto world = std::get<std::unique_ptr<physics::World>>(std::move(created));
    const auto admitted = world->add_box(definition.hull);
    if (const auto *error = std::get_if<Error>(&admitted))
        return *error;
    for (const auto &description : static_obstacles) {
        const auto obstacle = world->add_box(description);
        if (const auto *error = std::get_if<Error>(&obstacle))
            return *error;
    }
    auto impl = std::make_unique<Impl>();
    impl->world = std::move(world);
    impl->ship = definition;
    std::copy(static_obstacles.begin(), static_obstacles.end(), impl->obstacles.begin());
    impl->obstacle_count = static_obstacles.size();
    impl->body = std::get<physics::BodyId>(admitted);
    impl->published.entity = {0, 1};
    impl->published.body = std::get<physics::BodyState>(impl->world->state(impl->body));
    impl->published.flight_camera = definition.flight_camera;
    return std::unique_ptr<Flight>(new Flight(std::move(impl)));
}
Snapshot Flight::snapshot() const { return impl_->published; }
Result<Snapshot> Flight::reset() {
    if (owner_ != std::this_thread::get_id())
        return Error::order;
    const auto generation = next_generation(impl_->published.entity.generation);
    if (const auto *error = std::get_if<Error>(&generation))
        return *error;
    auto created =
        Flight::create(impl_->ship, std::span(impl_->obstacles).first(impl_->obstacle_count));
    if (const auto *error = std::get_if<Error>(&created))
        return *error;
    auto replacement = std::get<std::unique_ptr<Flight>>(std::move(created));
    replacement->impl_->published.entity = {impl_->published.entity.slot,
                                            std::get<std::uint64_t>(generation)};
    impl_.swap(replacement->impl_);
    return impl_->published;
}
Result<std::monostate> Flight::submit(const Command &command) {
    if (owner_ != std::this_thread::get_id())
        return Error::order;
    if (impl_->failed)
        return Error::closed;
    if (impl_->command_count == command_capacity)
        return Error::capacity;
    const auto upcoming = next(impl_->published.tick);
    if (const auto *error = std::get_if<Error>(&upcoming))
        return *error;
    if (command.tick != std::get<Tick>(upcoming) || command.sequence <= impl_->sequence)
        return Error::order;
    if (command.entity != impl_->published.entity)
        return Error::stale;
    if (!valid(command.intent))
        return Error::invalid;
    impl_->commands[impl_->command_count++] = command;
    impl_->sequence = command.sequence;
    return std::monostate{};
}
Result<Snapshot> Flight::step() {
    if (owner_ != std::this_thread::get_id())
        return Error::order;
    if (impl_->failed)
        return Error::closed;
    auto mode = impl_->published.mode;
    auto pilot = impl_->pilot;
    auto navigation = impl_->published.navigation;
    auto body = impl_->published.body;
    for (std::size_t i = 0; i < impl_->command_count; ++i) {
        const auto &intent = impl_->commands[i].intent;
        if (const auto *set_mode = std::get_if<SetMode>(&intent)) {
            // Manual takeover cancels old navigation instead of parking stale autopilot intent.
            if (mode != set_mode->mode && set_mode->mode == Mode::direct)
                navigation = {};
            mode = set_mode->mode;
        } else if (const auto *set_pilot = std::get_if<Pilot>(&intent))
            pilot = *set_pilot;
        else if (const auto *destination = std::get_if<Destination>(&intent)) {
            navigation = {destination->position_metres, 0, 0, Goal::destination, false};
            mode = Mode::tactical;
            pilot.brake = false;
        } else if (const auto *orbit = std::get_if<Orbit>(&intent)) {
            navigation = {orbit->centre_metres, orbit->radius_metres, orbit->speed_mps, Goal::orbit,
                          orbit->clockwise};
            mode = Mode::tactical;
            pilot.brake = false;
        } else if (const auto *set_mass = std::get_if<SetMass>(&intent)) {
            body.mass_kg = set_mass->kilograms;
        }
    }
    // Validate the complete candidate before changing native mass or applying a wrench.
    if (mode == Mode::tactical && !feasible(impl_->ship, navigation, body.mass_kg)) {
        impl_->command_count = 0;
        return Error::invalid;
    }
    if (body.mass_kg != impl_->published.body.mass_kg) {
        const auto changed = impl_->world->set_mass(impl_->body, body.mass_kg);
        if (const auto *error = std::get_if<Error>(&changed)) {
            impl_->failed = true;
            return *error;
        }
    }
    const auto requested = control_wrench(impl_->ship, body, pilot, mode, navigation);
    auto staged_pulses = impl_->pulses;
    const auto actuation = actuate(impl_->ship, body, requested, staged_pulses);
    const auto &wrench = actuation.wrench;
    const auto applied = impl_->world->apply(impl_->body, wrench.force, wrench.torque);
    if (const auto *error = std::get_if<Error>(&applied)) {
        impl_->failed = true;
        return *error;
    }
    const auto advanced = impl_->world->step();
    if (const auto *error = std::get_if<Error>(&advanced)) {
        impl_->failed = true;
        return *error;
    }
    Snapshot result = impl_->published;
    result.tick = std::get<Tick>(advanced);
    result.mode = mode;
    result.navigation = navigation;
    result.force_newtons = wrench.force;
    result.torque_newton_metres = wrench.torque;
    result.body = std::get<physics::BodyState>(impl_->world->state(impl_->body));
    result.rcs_firing = actuation.firing;
    result.main_drive_level = actuation.main;
    result.rcs_demand_level = actuation.demand;
    impl_->pulses = staged_pulses;
    impl_->pilot = pilot;
    impl_->published = result;
    impl_->command_count = 0;
    return result;
}
} // namespace ngen::flight
