#include <Jolt/Jolt.h>

#include "flight/flight.hpp"
#include <algorithm>
#include <cmath>
#include <thread>
#include <type_traits>

namespace ngen::flight {
namespace {
bool valid(const ShipDefinition &ship) {
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
    if (force.LengthSq() > 0) {
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
        // Scale the admitted wrench, never invent braking on an unavailable axis.
        // Existing collision-induced overspeed may decelerate, not gain energy.
        const auto limit = static_cast<double>(ship.maximum_speed_mps);
        const auto c = speed_squared - std::max(speed_squared, limit * limit);
        if (a > 0 && a + b + c > 0) {
            const auto scale = std::clamp((-b + std::sqrt(b * b - 4 * a * c)) / (2 * a), 0.0, 1.0);
            force *= static_cast<float>(scale);
        }
    }
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
        if (const auto *value = std::get_if<SetMode>(&intent))
            mode = value->mode;
        else if (const auto *value = std::get_if<Pilot>(&intent))
            pilot = *value;
        else if (const auto *value = std::get_if<Destination>(&intent)) {
            navigation = {value->position_metres, 0, 0, Goal::destination, false};
            mode = Mode::tactical;
            pilot.brake = false;
        } else if (const auto *value = std::get_if<Orbit>(&intent)) {
            navigation = {value->centre_metres, value->radius_metres, value->speed_mps, Goal::orbit,
                          value->clockwise};
            mode = Mode::tactical;
            pilot.brake = false;
        } else if (const auto *value = std::get_if<SetMass>(&intent)) {
            body.mass_kg = value->kilograms;
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
    const auto wrench = control_wrench(impl_->ship, body, pilot, mode, navigation);
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
    impl_->pilot = pilot;
    impl_->published = result;
    impl_->command_count = 0;
    return result;
}
} // namespace ngen::flight
