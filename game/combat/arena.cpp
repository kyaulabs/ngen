#include "combat/arena.hpp"
#include <algorithm>

namespace ngen::combat {
namespace {
struct Segment {
    Point start, end;
};
struct Box {
    Point min, max;
};

bool valid(Ship ship) { return ship == Ship::player || ship == Ship::rival; }

bool in_range(Segment path, int radius) {
    const auto x = path.end.x - path.start.x;
    const auto y = path.end.y - path.start.y;
    const auto z = path.end.z - path.start.z;
    return x * x + y * y + z * z <= radius * radius;
}

// Closed fixture boxes: a tangent or an endpoint on the boundary counts.
std::optional<double> entry(Segment path, Box box) {
    double enter = 0;
    double leave = 1;
    const auto clip = [&](int Point::*axis) {
        const int from = path.start.*axis;
        const int delta = path.end.*axis - from;
        const int low = box.min.*axis;
        const int high = box.max.*axis;
        if (!delta)
            return low <= from && from <= high;
        const auto first = static_cast<double>(low - from) / delta;
        const auto last = static_cast<double>(high - from) / delta;
        enter = std::max(enter, std::min(first, last));
        leave = std::min(leave, std::max(first, last));
        return enter <= leave;
    };
    if (!clip(&Point::x) || !clip(&Point::y) || !clip(&Point::z))
        return std::nullopt;
    return enter;
}
} // namespace

std::unique_ptr<Arena> Arena::create(Fixture fixture) {
    if (fixture.separation_metres < 8 || fixture.separation_metres > 128)
        return {};
    return std::unique_ptr<Arena>(new Arena(fixture));
}

Point Arena::center(Ship ship) const {
    return {ship == Ship::player ? 0 : fixture_.separation_metres};
}

Health &Arena::ship_health(Ship ship) {
    return ship == Ship::player ? health_.player : health_.rival;
}

Impact Arena::trace(Ship shooter, Point end) const {
    const auto target = shooter == Ship::player ? Ship::rival : Ship::player;
    const auto x = center(target).x;
    const Segment path{center(shooter), end};
    const auto hull = entry(path, {{x - 2, -2, -2}, {x + 2, 2, 2}});
    if (fixture_.cover) {
        const auto mid = fixture_.separation_metres / 2;
        const auto wall = entry(path, {{mid - 1, -3, -3}, {mid + 1, 3, 3}});
        if (wall && (!hull || *wall <= *hull))
            return {Hit::cover};
    }
    if (!hull)
        return {};
    const auto upper = entry(path, {{x - 2, 1, -2}, {x + 2, 2, 2}});
    const auto lower = entry(path, {{x - 2, -2, -2}, {x + 2, -1, 2}});
    // Only equipment at the first hull contact can receive this impact.
    if (upper && *upper == *hull)
        return {Hit::ship, target, Region::targeted_mount};
    if (lower && *lower == *hull)
        return {Hit::ship, target, Region::projectile_mount};
    return {Hit::ship, target};
}

Impact Arena::damage(Impact impact, unsigned amount) {
    if (impact.hit != Hit::ship)
        return impact;
    auto &health = ship_health(impact.ship);
    if (!health.hull)
        return impact;
    auto &remaining = impact.region == Region::targeted_mount     ? health.targeted_mount
                      : impact.region == Region::projectile_mount ? health.projectile_mount
                                                                  : health.hull;
    impact.applied = std::min(remaining, amount);
    remaining -= impact.applied;
    return impact;
}

std::variant<Impact, Reject> Arena::fire(TargetedShot shot) {
    if (!valid(shot.shooter))
        return Reject::invalid_shooter;
    if (!valid(shot.target) || shot.target == shot.shooter || !ship_health(shot.target).hull)
        return Reject::invalid_target;
    const auto &shooter = ship_health(shot.shooter);
    if (!shooter.hull || !shooter.targeted_mount)
        return Reject::disabled;
    auto end = center(shot.target);
    switch (shot.aim) {
    case Region::hull:
        break;
    case Region::targeted_mount:
        end.y = 2;
        break;
    case Region::projectile_mount:
        end.y = -2;
        break;
    default:
        return Reject::invalid_target;
    }
    if (!in_range({center(shot.shooter), end}, 40))
        return Reject::out_of_range;
    const auto impact = trace(shot.shooter, end);
    if (impact.hit != Hit::ship)
        return Reject::blocked;
    return damage(impact, 20);
}

std::optional<Reject> Arena::launch(ProjectileShot shot) {
    if (!valid(shot.shooter))
        return Reject::invalid_shooter;
    const auto &shooter = ship_health(shot.shooter);
    if (!shooter.hull || !shooter.projectile_mount)
        return Reject::disabled;
    constexpr auto bounded = [](int v) { return v >= -256 && v <= 256; };
    const auto start = center(shot.shooter);
    if (!bounded(shot.end.x) || !bounded(shot.end.y) || !bounded(shot.end.z) || shot.end == start ||
        !in_range({start, shot.end}, 96))
        return Reject::invalid_path;
    if (used_ == pending_.size())
        return Reject::full;
    pending_[used_++] = shot;
    return std::nullopt;
}

Arena::TickReport Arena::advance() {
    TickReport report{.size = used_};
    for (std::size_t i = 0; i < used_; ++i) {
        const auto shot = pending_[i];
        report.impacts[i] = damage(trace(shot.shooter, shot.end), 35);
    }
    used_ = 0;
    return report;
}
} // namespace ngen::combat
