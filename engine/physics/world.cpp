#include <Jolt/Jolt.h>

#include "physics/world.hpp"
#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/StaticCompoundShape.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
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
bool position_fits(const std::array<double, 3> &position) {
    return std::all_of(position.begin(), position.end(),
                       [](double x) { return std::abs(x) <= 8192; });
}
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
    coordinates::Frame frame;
    JPH::TempAllocatorImpl temporary{std::size_t{8} * 1024 * 1024};
    std::array<Slot, body_capacity> slots{};
    std::array<JPH::Ref<JPH::TwoBodyConstraint>, body_capacity> constraints{};
    std::size_t constraint_count{};
    bool failed{};
    // Destroy the system before its borrowed filters, scratch and job owner.
    JPH::PhysicsSystem physics;
    Impl(std::uint64_t id, const coordinates::Position &origin)
        : frame{.stamp = {id, 1, {}}, .origin = origin} {
        physics.Init(body_capacity, 0, 8192, 4096, broadphase, object_broadphase, object_pair);
        physics.SetGravity(JPH::Vec3::sZero());
    }
    ~Impl() {
        for (std::size_t i = 0; i < constraint_count; ++i) {
            physics.RemoveConstraint(constraints[i].GetPtr());
            constraints[i] = nullptr;
        }
        auto &bodies = physics.GetBodyInterface();
        for (const auto &slot : slots)
            if (slot.live) {
                bodies.RemoveBody(slot.native);
                bodies.DestroyBody(slot.native);
            }
    }
    bool owns_thread() const { return owner == std::this_thread::get_id(); }
    bool valid(BodyId id) const {
        return id.world == frame.stamp.id && id.slot < slots.size() && slots[id.slot].live &&
               slots[id.slot].generation == id.generation;
    }
};
World::World(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
World::~World() = default;
Result<std::unique_ptr<World>> World::create(coordinates::Position origin) {
    const auto canonical = coordinates::normalize(origin);
    if (const auto *error = std::get_if<Error>(&canonical))
        return *error;
    if (std::get<coordinates::Position>(canonical) != origin)
        return Error::invalid;
    try {
        static Runtime runtime;
        static std::atomic<std::uint64_t> domains{};
        auto domain = domains.load();
        do {
            if (domain == std::numeric_limits<std::uint64_t>::max())
                return Error::overflow;
        } while (!domains.compare_exchange_weak(domain, domain + 1));
        return std::unique_ptr<World>(new World(std::make_unique<Impl>(domain + 1, origin)));
    } catch (const std::bad_alloc &) {
        return Error::capacity;
    }
}
Result<BodyId> World::add_two_section_hull(const Box &section) { return add_shape(section, true); }
Result<std::monostate> World::constrain_distance(BodyId a, BodyId b, float metres) {
    if (!impl_->owns_thread())
        return Error::order;
    if (impl_->failed)
        return Error::closed;
    if (!impl_->valid(a) || !impl_->valid(b))
        return Error::stale;
    const auto &first = impl_->slots[a.slot];
    const auto &second = impl_->slots[b.slot];
    if (a == b || (!first.dynamic && !second.dynamic) || !(metres >= 0.1F && metres <= 8192))
        return Error::invalid;
    if (impl_->constraint_count == impl_->constraints.size())
        return Error::capacity;

    JPH::DistanceConstraintSettings settings;
    settings.mSpace = JPH::EConstraintSpace::LocalToBodyCOM;
    settings.mMinDistance = metres;
    settings.mMaxDistance = metres;
    auto &bodies = impl_->physics.GetBodyInterface();
    JPH::Ref<JPH::TwoBodyConstraint> constraint =
        bodies.CreateConstraint(&settings, first.native, second.native);
    if (constraint == nullptr)
        return Error::invalid;
    impl_->physics.AddConstraint(constraint.GetPtr());
    impl_->constraints[impl_->constraint_count++] = constraint;
    return std::monostate{};
}
Result<bool> World::in_contact(BodyId a, BodyId b) const {
    if (!impl_->owns_thread())
        return Error::order;
    if (impl_->failed)
        return Error::closed;
    if (!impl_->valid(a) || !impl_->valid(b))
        return Error::stale;
    if (a == b)
        return Error::invalid;
    return impl_->physics.WereBodiesInContact(impl_->slots[a.slot].native,
                                              impl_->slots[b.slot].native);
}
Result<BodyId> World::add_box(const Box &description) { return add_shape(description, false); }
Result<BodyId> World::add_shape(const Box &description, bool two_sections) {
    if (!impl_->owns_thread())
        return Error::order;
    if (impl_->failed)
        return Error::closed;
    if (!std::all_of(description.half_extent_metres.begin(), description.half_extent_metres.end(),
                     [](float v) { return v >= .1F && v <= 256; }) ||
        (two_sections && description.half_extent_metres[0] > 250) ||
        !position_fits(description.position_metres) || !std::isfinite(description.mass_kg) ||
        description.mass_kg < 1 || description.mass_kg > 1e12)
        return Error::invalid;
    auto at = std::find_if(impl_->slots.begin(), impl_->slots.end(),
                           [](const Impl::Slot &s) { return !s.live; });
    if (at == impl_->slots.end())
        return Error::capacity;
    const auto &p = description.position_metres;
    const auto half = vector(description.half_extent_metres);
    JPH::BodyCreationSettings settings;
    settings.mPosition = {p[0], p[1], p[2]};
    settings.mMotionType =
        description.dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static;
    settings.mObjectLayer = description.dynamic ? 1 : 0;
    // The native body settings own the shape from construction to admission.
    // The compound's children are colliders of that one body, not separate bodies.
    if (two_sections) {
        const auto x = half.GetX();
        auto *sections = new JPH::StaticCompoundShapeSettings;
        settings.SetShapeSettings(sections);
        sections->AddShape({-x, 0, 0}, JPH::Quat::sIdentity(), new JPH::BoxShape(half));
        sections->AddShape({x, 0, 0}, JPH::Quat::sIdentity(), new JPH::BoxShape(half));
        if (!settings.ConvertShapeSettings().IsValid())
            return Error::invalid;
    } else {
        settings.SetShape(new JPH::BoxShape(half));
    }
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
                  impl_->frame.stamp.id};
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
    const auto upcoming = next(impl_->frame.stamp.tick);
    if (const auto *error = std::get_if<Error>(&upcoming))
        return *error;
    if (impl_->physics.Update(1.0F / ticks_per_second, 1, &impl_->temporary, &impl_->jobs) !=
        JPH::EPhysicsUpdateError::None) {
        impl_->failed = true;
        return Error::capacity;
    }
    impl_->frame.stamp.tick = std::get<Tick>(upcoming);
    return impl_->frame.stamp.tick;
}
Result<coordinates::Frame> World::frame() const {
    if (!impl_->owns_thread())
        return Error::order;
    if (impl_->failed)
        return Error::closed;
    return impl_->frame;
}
Result<coordinates::Frame> World::rebase(coordinates::FrameStamp expected,
                                         coordinates::Position origin) {
    if (!impl_->owns_thread())
        return Error::order;
    if (impl_->failed)
        return Error::closed;
    const auto &source = impl_->frame;
    if (expected != source.stamp)
        return Error::stale;
    if (origin == source.origin)
        return Error::invalid;
    const auto epoch = next_generation(source.stamp.epoch);
    if (const auto *error = std::get_if<Error>(&epoch))
        return *error;
    auto target = source;
    target.stamp.epoch = std::get<std::uint64_t>(epoch);
    target.origin = origin;
    const auto moved_origin = coordinates::transform({.stamp = expected}, source, target);
    if (const auto *error = std::get_if<Error>(&moved_origin))
        return *error;
    const auto &delta = std::get<coordinates::Sample>(moved_origin).position_metres;

    // Preflight every live slot before mutating native state. Body identity,
    // velocities, accumulated forces and local constraint anchors stay native.
    std::array<std::array<double, 3>, body_capacity> positions{};
    auto &bodies = impl_->physics.GetBodyInterface();
    for (std::size_t index = 0; index < impl_->slots.size(); ++index) {
        const auto &slot = impl_->slots[index];
        if (!slot.live)
            continue;
        const auto p = bodies.GetPosition(slot.native);
        auto &out = positions[index];
        out = {p.GetX() + delta[0], p.GetY() + delta[1], p.GetZ() + delta[2]};
        if (!position_fits(out))
            return Error::invalid;
    }
    for (std::size_t index = 0; index < impl_->slots.size(); ++index) {
        const auto &slot = impl_->slots[index];
        if (!slot.live)
            continue;
        const auto &p = positions[index];
        bodies.SetPosition(slot.native, {p[0], p[1], p[2]}, JPH::EActivation::DontActivate);
    }
    impl_->frame = target;
    return target;
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
                     impl_->frame.stamp.tick,
                     {p.GetX(), p.GetY(), p.GetZ()},
                     {q.GetX(), q.GetY(), q.GetZ(), q.GetW()},
                     {v.GetX(), v.GetY(), v.GetZ()},
                     {w.GetX(), w.GetY(), w.GetZ()},
                     slot.mass,
                     impl_->frame.stamp};
}
} // namespace ngen::physics
