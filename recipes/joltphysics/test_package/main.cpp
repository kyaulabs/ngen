#include <Jolt/Jolt.h>

#include <Jolt/Core/Factory.h>
#include <Jolt/RegisterTypes.h>
#include <cmath>
#include <cstdio>
#include <memory>

int main() {
    JPH::RegisterDefaultAllocator();
    auto factory = std::make_unique<JPH::Factory>();
    JPH::Factory::sInstance = factory.get();
    JPH::RegisterTypes();
    // Exercise declared local-position precision, not only sizeof/a macro.
    constexpr double coordinate = 4096.001;
    const JPH::RVec3 position{static_cast<JPH::Real>(coordinate), 0, 0};
    const bool precise = std::abs(position.GetX() - coordinate) < 1e-8;
    JPH::UnregisterTypes();
    JPH::Factory::sInstance = nullptr;
    if (!precise) {
        std::fputs("Jolt package lost local millimetre position precision\n", stderr);
        return 1;
    }
    std::puts("headless_jolt_registered=1 double_local_position=1");
}
