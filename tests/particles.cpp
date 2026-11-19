#include "effects/particles.hpp"
#include <catch2/catch_test_macros.hpp>
#include <limits>

using namespace ngen;
using namespace ngen::effects;

TEST_CASE("cosmetic particles move at fixed ticks and expire without changing earlier snapshots",
          "[particles]") {
    const coordinates::Frame frame{.stamp = {7, 1, Tick{0}}, .origin = {}};
    auto system = std::get<Particles>(Particles::create(frame));
    REQUIRE(system.emit({frame.stamp, {1, 2, 3}, {60, 0, 0}}, 2) ==
            Result<std::monostate>{std::monostate{}});
    CHECK(system.step() == Result<Tick>{Tick{1}});
    const auto saved = system.snapshot();
    REQUIRE(saved.particles().size() == 1);
    CHECK(saved.particles()[0].position_metres == std::array{2.0, 2.0, 3.0});
    CHECK(saved.particles()[0].age_ticks == 1);
    CHECK(system.step() == Result<Tick>{Tick{2}});
    const auto expired = system.snapshot();
    CHECK(expired.particles().empty());
    CHECK(saved.particles().size() == 1);
}

TEST_CASE("a stationary origin change preserves the particle trajectory and lifetime",
          "[particles]") {
    const coordinates::Frame start{.stamp = {7, 1, Tick{0}},
                                   .origin = {{{145'000'000'000, 0x1p31 - 128}, {}, {}}}};
    auto control = std::get<Particles>(Particles::create(start));
    auto system = std::get<Particles>(Particles::create(start));
    for (auto *pool : {&control, &system}) {
        REQUIRE(pool->emit({start.stamp, {1, 2, 3}, {60, -60, 30}}, 3) ==
                Result<std::monostate>{std::monostate{}});
        REQUIRE(pool->step() == Result<Tick>{Tick{1}});
    }
    auto target = system.snapshot().frame;
    auto stale = target;
    ++stale.stamp.id;
    CHECK(system.rebase(stale) == Result<std::monostate>{Error::stale});
    ++target.stamp.epoch;
    target.origin = {{{145'000'000'001, -0x1p31 + 128}, {}, {}}};
    REQUIRE(system.rebase(target) == Result<std::monostate>{std::monostate{}});
    CHECK(system.snapshot().frame.stamp == target.stamp);
    REQUIRE(control.step() == system.step());
    const auto reference = control.snapshot();
    const auto translated = system.snapshot();
    REQUIRE(reference.particles().size() == 1);
    REQUIRE(translated.particles().size() == 1);
    const auto &a = reference.particles()[0];
    const auto &b = translated.particles()[0];
    CHECK(a.position_metres[0] == b.position_metres[0] + 256);
    CHECK(a.position_metres[1] == b.position_metres[1]);
    CHECK(a.position_metres[2] == b.position_metres[2]);
    CHECK(a.velocity_mps == b.velocity_mps);
    CHECK(a.age_ticks == b.age_ticks);
    CHECK(a.lifetime_ticks == b.lifetime_ticks);
}

TEST_CASE("stale and excess emissions cannot replace existing particles", "[particles]") {
    const coordinates::Frame frame{.stamp = {7, 3, Tick{18}}, .origin = {}};
    auto pool = std::get<Particles>(Particles::create(frame));
    auto sample = coordinates::Sample{frame.stamp, {1, 2, 3}, {}};
    CHECK(pool.emit(sample, 0) == Result<std::monostate>{Error::invalid});
    CHECK(pool.emit(sample, Particles::max_lifetime_ticks + 1) ==
          Result<std::monostate>{Error::invalid});
    --sample.stamp.epoch;
    CHECK(pool.emit(sample, 1) == Result<std::monostate>{Error::stale});
    sample.stamp = frame.stamp;
    sample.velocity_mps[1] = std::numeric_limits<double>::quiet_NaN();
    CHECK(pool.emit(sample, 1) == Result<std::monostate>{Error::invalid});
    sample.velocity_mps[1] = 0;
    for (std::size_t i = 0; i < ParticleSnapshot::capacity; ++i)
        REQUIRE(pool.emit(sample, 1) == Result<std::monostate>{std::monostate{}});
    sample.position_metres[0] = 10;
    CHECK(pool.emit(sample, 1) == Result<std::monostate>{Error::capacity});
    const auto full = pool.snapshot();
    REQUIRE(full.particles().size() == ParticleSnapshot::capacity);
    for (const auto &particle : full.particles())
        CHECK(particle.position_metres[0] == 1);
    CHECK(pool.step() == Result<Tick>{Tick{19}});
    const auto empty = pool.snapshot();
    CHECK(empty.particles().empty());
}

TEST_CASE("an out of range particle conversion cannot publish an earlier partial update",
          "[particles]") {
    const coordinates::Frame frame{.stamp = {7, 1, Tick{0}}, .origin = {}};
    auto pool = std::get<Particles>(Particles::create(frame));
    REQUIRE(pool.emit({frame.stamp, {0, 0, 0}, {}}, 2) == Result<std::monostate>{std::monostate{}});
    REQUIRE(pool.emit({frame.stamp, {coordinates::local_limit_metres, 0, 0}, {60, 0, 0}}, 2) ==
            Result<std::monostate>{std::monostate{}});
    auto target = frame;
    ++target.stamp.epoch;
    target.origin[0].offset_metres = -1;
    CHECK(pool.rebase(target) == Result<std::monostate>{Error::invalid});
    CHECK(pool.step() == Result<Tick>{Error::invalid});
    const auto state = pool.snapshot();
    CHECK(state.frame.stamp == frame.stamp);
    REQUIRE(state.particles().size() == 2);
    CHECK(state.particles()[0].position_metres[0] == 0);
    CHECK(state.particles()[0].age_ticks == 0);
}
