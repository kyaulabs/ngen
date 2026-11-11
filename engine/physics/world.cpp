#include <Jolt/Jolt.h>

#include "physics/world.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/RegisterTypes.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <limits>
#include <thread>

namespace ngen::physics {
namespace {
class Runtime {
  public:
    Runtime() {
        JPH::RegisterDefaultAllocator();
        factory_ = std::make_unique<JPH::Factory>();
        JPH::Factory::sInstance = factory_.get();
        JPH::RegisterTypes();
    }
    ~Runtime() {
        JPH::UnregisterTypes();
        JPH::Factory::sInstance = nullptr;
    }
    Runtime(const Runtime &) = delete;
    Runtime &operator=(const Runtime &) = delete;

  private:
    std::unique_ptr<JPH::Factory> factory_;
};
class Broadphase final : public JPH::BroadPhaseLayerInterface {
    JPH::uint GetNumBroadPhaseLayers() const override { return 2; }
    JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer layer) const override {
        return JPH::BroadPhaseLayer(static_cast<JPH::BroadPhaseLayer::Type>(layer));
    }
};
class ObjectBroadphase final : public JPH::ObjectVsBroadPhaseLayerFilter {
    bool ShouldCollide(JPH::ObjectLayer object, JPH::BroadPhaseLayer broadphase) const override {
        return object != 0 || broadphase.GetValue() != 0;
    }
};
class ObjectPair final : public JPH::ObjectLayerPairFilter {
    bool ShouldCollide(JPH::ObjectLayer a, JPH::ObjectLayer b) const override {
        return a != 0 || b != 0;
    }
};
JPH::Vec3 vector(std::array<float, 3> value) { return {value[0], value[1], value[2]}; }
struct Wrench {
    JPH::Vec3 force, torque;
};
bool motion_fits(const JPH::Body &body, const Wrench &wrench, float mass_scale = 1) {
    const auto *motion = body.GetMotionProperties();
    const auto linear = body.GetLinearVelocity() +
                        (body.GetAccumulatedForce() + wrench.force) *
                            (motion->GetInverseMass() * mass_scale / World::ticks_per_second);
    const auto angular =
        body.GetAngularVelocity() +
        body.GetInverseInertia().Multiply3x3(body.GetAccumulatedTorque() + wrench.torque) *
            (mass_scale / World::ticks_per_second);
    const auto speed_squared = linear.LengthSq(), spin_squared = angular.LengthSq();
    return std::isfinite(speed_squared) && std::isfinite(spin_squared) &&
           speed_squared <= motion->GetMaxLinearVelocity() * motion->GetMaxLinearVelocity() &&
           spin_squared <= motion->GetMaxAngularVelocity() * motion->GetMaxAngularVelocity();
}
} // namespace
struct World::Impl {
    struct Slot {
        JPH::BodyID native;
        std::uint64_t generation{};
        double mass{};
        bool dynamic{}, live{};
    };
    JPH::JobSystemSingleThreaded jobs{1024};
    Broadphase broadphase;
    ObjectBroadphase object_broadphase;
    ObjectPair object_pair;
    std::thread::id owner{std::this_thread::get_id()};
    std::uint64_t domain{};
    Tick tick{};
    JPH::TempAllocatorImpl temporary{std::size_t{8} * 1024 * 1024};
    std::array<Slot, body_capacity> slots{};
    bool failed{};
    // Destroy the system before its borrowed filters, scratch and job owner.
    JPH::PhysicsSystem physics;
    explicit Impl(std::uint64_t id) : domain(id) {
        physics.Init(body_capacity, 0, 8192, 4096, broadphase, object_broadphase, object_pair);
        physics.SetGravity(JPH::Vec3::sZero());
    }
    ~Impl() {
        auto &bodies = physics.GetBodyInterface();
        for (const auto &slot : slots)
            if (slot.live) {
                bodies.RemoveBody(slot.native);
                bodies.DestroyBody(slot.native);
            }
    }
    bool owns_thread() const { return owner == std::this_thread::get_id(); }
    bool valid(BodyId id) const {
        return id.world == domain && id.slot < slots.size() && slots[id.slot].live &&
               slots[id.slot].generation == id.generation;
    }
};
World::World(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
World::~World() = default;
Result<std::unique_ptr<World>> World::create() {
    try {
        static Runtime runtime;
        static std::atomic<std::uint64_t> domains{};
        auto domain = domains.load();
        do {
            if (domain == std::numeric_limits<std::uint64_t>::max())
                return Error::overflow;
        } while (!domains.compare_exchange_weak(domain, domain + 1));
        return std::unique_ptr<World>(new World(std::make_unique<Impl>(domain + 1)));
    } catch (const std::bad_alloc &) {
        return Error::capacity;
    }
}
Result<BodyId> World::add_box(const Box &description) {
    if (!impl_->owns_thread())
        return Error::order;
    if (impl_->failed)
        return Error::closed;
    if (!std::all_of(description.half_extent_metres.begin(), description.half_extent_metres.end(),
                     [](float v) { return std::isfinite(v) && v >= .1F && v <= 256; }) ||
        !std::all_of(description.position_metres.begin(), description.position_metres.end(),
                     [](double v) { return std::isfinite(v) && std::abs(v) <= 8192; }) ||
        !std::isfinite(description.mass_kg) || description.mass_kg < 1 ||
        description.mass_kg > 1e12)
        return Error::invalid;
    auto at = std::find_if(impl_->slots.begin(), impl_->slots.end(),
                           [](const Impl::Slot &s) { return !s.live; });
    if (at == impl_->slots.end())
        return Error::capacity;
    JPH::BoxShapeSettings shape(vector(description.half_extent_metres));
    auto created = shape.Create();
    if (created.HasError())
        return Error::invalid;
    const auto &p = description.position_metres;
    JPH::BodyCreationSettings settings(created.Get(), {p[0], p[1], p[2]}, JPH::Quat::sIdentity(),
                                       description.dynamic ? JPH::EMotionType::Dynamic
                                                           : JPH::EMotionType::Static,
                                       description.dynamic ? 1 : 0);
    settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
    settings.mMassPropertiesOverride.mMass = static_cast<float>(description.mass_kg);
    settings.mLinearDamping = settings.mAngularDamping = 0;
    settings.mAllowSleeping = false;
    settings.mFriction = settings.mRestitution = 0;
    settings.mMotionQuality = JPH::EMotionQuality::LinearCast;
    const auto native = impl_->physics.GetBodyInterface().CreateAndAddBody(
        settings,
        description.dynamic ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    if (native.IsInvalid())
        return Error::capacity;
    at->native = native;
    ++at->generation;
    at->mass = description.mass_kg;
    at->dynamic = description.dynamic;
    at->live = true;
    return BodyId{static_cast<std::uint32_t>(at - impl_->slots.begin()), at->generation,
                  impl_->domain};
}
Result<std::monostate> World::apply(BodyId id, std::array<float, 3> force,
                                    std::array<float, 3> torque) {
    if (!impl_->owns_thread())
        return Error::order;
    if (impl_->failed)
        return Error::closed;
    if (!impl_->valid(id))
        return Error::stale;
    if (!impl_->slots[id.slot].dynamic ||
        !std::all_of(force.begin(), force.end(),
                     [](float v) { return std::isfinite(v) && std::abs(v) <= 1e12F; }) ||
        !std::all_of(torque.begin(), torque.end(),
                     [](float v) { return std::isfinite(v) && std::abs(v) <= 1e15F; }))
        return Error::invalid;
    {
        JPH::BodyLockRead lock(impl_->physics.GetBodyLockInterface(), impl_->slots[id.slot].native);
        if (!lock.Succeeded())
            return Error::stale;
        if (!motion_fits(lock.GetBody(), {.force = vector(force), .torque = vector(torque)}))
            return Error::invalid;
    }
    impl_->physics.GetBodyInterface().AddForceAndTorque(impl_->slots[id.slot].native, vector(force),
                                                        vector(torque));
    return std::monostate{};
}
Result<std::monostate> World::set_mass(BodyId id, double kilograms) {
    if (!impl_->owns_thread())
        return Error::order;
    if (impl_->failed)
        return Error::closed;
    if (!impl_->valid(id))
        return Error::stale;
    if (!impl_->slots[id.slot].dynamic || !std::isfinite(kilograms) || kilograms < 1 ||
        kilograms > 1e12)
        return Error::invalid;
    JPH::BodyLockWrite lock(impl_->physics.GetBodyLockInterface(), impl_->slots[id.slot].native);
    if (!lock.Succeeded())
        return Error::stale;
    auto &body = lock.GetBody();
    auto *motion = body.GetMotionProperties();
    const auto scale = (1.0F / static_cast<float>(kilograms)) / motion->GetInverseMass();
    if (!motion_fits(body, {.force = JPH::Vec3::sZero(), .torque = JPH::Vec3::sZero()}, scale))
        return Error::invalid;
    motion->ScaleToMass(static_cast<float>(kilograms));
    impl_->slots[id.slot].mass = kilograms;
    return std::monostate{};
}
Result<Tick> World::step() {
    if (!impl_->owns_thread())
        return Error::order;
    if (impl_->failed)
        return Error::closed;
    const auto upcoming = next(impl_->tick);
    if (const auto *error = std::get_if<Error>(&upcoming))
        return *error;
    if (impl_->physics.Update(1.0F / ticks_per_second, 1, &impl_->temporary, &impl_->jobs) !=
        JPH::EPhysicsUpdateError::None) {
        impl_->failed = true;
        return Error::capacity;
    }
    impl_->tick = std::get<Tick>(upcoming);
    return impl_->tick;
}
Result<BodyState> World::state(BodyId id) const {
    if (!impl_->owns_thread())
        return Error::order;
    if (impl_->failed)
        return Error::closed;
    if (!impl_->valid(id))
        return Error::stale;
    const auto &slot = impl_->slots[id.slot];
    const auto &bodies = impl_->physics.GetBodyInterface();
    const auto p = bodies.GetPosition(slot.native);
    const auto q = bodies.GetRotation(slot.native);
    const auto v = bodies.GetLinearVelocity(slot.native);
    const auto w = bodies.GetAngularVelocity(slot.native);
    return BodyState{id,
                     impl_->tick,
                     {p.GetX(), p.GetY(), p.GetZ()},
                     {q.GetX(), q.GetY(), q.GetZ(), q.GetW()},
                     {v.GetX(), v.GetY(), v.GetZ()},
                     {w.GetX(), w.GetY(), w.GetZ()},
                     slot.mass};
}
} // namespace ngen::physics
