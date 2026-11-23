#pragma once
#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <variant>

namespace ngen::combat {
enum class Ship : std::uint8_t { none, player, rival };
enum class Region : std::uint8_t { hull, targeted_mount, projectile_mount };
struct Point {
    int x{}, y{}, z{};
    auto operator<=>(const Point &) const = default;
};
struct Fixture {
    int separation_metres{24};
    bool cover{};
};
struct Health {
    unsigned hull{100}, targeted_mount{20}, projectile_mount{20};
    auto operator<=>(const Health &) const = default;
};
struct Snapshot {
    Health player, rival;
    auto operator<=>(const Snapshot &) const = default;
};
struct TargetedShot {
    Ship shooter{}, target{};
    Region aim{};
};
struct ProjectileShot {
    Ship shooter{};
    Point end;
};
enum class Hit : std::uint8_t { miss, cover, ship };
struct Impact {
    Hit hit{};
    Ship ship{};
    Region region{};
    unsigned applied{};
    auto operator<=>(const Impact &) const = default;
};
enum class Reject : std::uint8_t {
    invalid_shooter,
    invalid_target,
    disabled,
    out_of_range,
    blocked,
    invalid_path,
    full
};

// One game owner in a fixed local frame. No native body or worker owns health.
class Arena {
  public:
    static constexpr std::size_t projectile_capacity = 4;
    struct TickReport {
        std::array<Impact, projectile_capacity> impacts{};
        std::size_t size{};
    };

    [[nodiscard]] static std::unique_ptr<Arena> create(Fixture fixture);
    Arena(const Arena &) = delete;
    Arena &operator=(const Arena &) = delete;
    [[nodiscard]] Snapshot snapshot() const { return health_; }
    [[nodiscard]] std::variant<Impact, Reject> fire(TargetedShot shot);
    [[nodiscard]] std::optional<Reject> launch(ProjectileShot shot);
    // Resolve each accepted one-tick segment in admission order, exactly once.
    [[nodiscard]] TickReport advance();

  private:
    explicit Arena(Fixture fixture) : fixture_(fixture) {}
    [[nodiscard]] Point center(Ship ship) const;
    [[nodiscard]] Health &ship_health(Ship ship);
    [[nodiscard]] Impact trace(Ship shooter, Point end) const;
    [[nodiscard]] Impact damage(Impact impact, unsigned amount);

    Fixture fixture_;
    Snapshot health_;
    std::array<ProjectileShot, projectile_capacity> pending_{};
    std::size_t used_{};
};
} // namespace ngen::combat
