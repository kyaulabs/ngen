# Headless double-position Jolt

M3 needs a bounded local rigid-body/contact/query implementation, not a new
custom physics engine. Jolt5.6.0 is the existing architecture candidate. The
resolved Conan Center recipe revision `949c90f029bb1f7f9f377a04ab2b1146` exposes
only shared/fPIC options, cannot select double positions, and publishes the
CPU-compute interface. Changing hidden CMake variables without a distinct
package identity would risk mixing incompatible float/double binaries.

This minimal local `joltphysics/5.6.0@ngen/headless-double` recipe uses the same
upstream tag/archive SHA256, with no physics source patch. It fixes double
positions, static linkage,16-bit object layers and a CPU rigid-body-only build.
DX12/Vulkan/Metal and the optional CPU-compute interface, demos, profiler,
debug renderer and object stream are disabled. Baseline x86 SIMD avoids an AVX
requirement; exceptions/RTTI remain enabled and the Windows CRT follows the
actual profile. Package metadata exposes matching public defines. The upstream
MIT notice is copied into each package; NGEN's own license is unchanged.

The native package test links/registers Jolt and checks actual local millimetre
position precision. This does not validate whole-physics cross-platform replay,
capital hull contacts, astronomical coordinates or M4 frame transfers. Verify
headless linkage and required toolchains before accepting the adapter. No
parallel worker pool is introduced by this package configuration.
