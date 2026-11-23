# M5-C fixture combat

One offline `combat::Arena`, owned by the game session, holds health and accepted projectile segments. The fixture places two stationary ships and optional cover in a single local frame. Each hull has two fitted weapon hit regions; the regions are boxes, not additional rigid bodies. This does not add a general Jolt query or change live flight physics.

A targeted shot requires a live opposing target, an enabled targeted mount, an aim point within 40 metres, and clear line of sight. A projectile launch requires an enabled projectile mount and a valid, nonempty segment up to 96 metres. The next `advance()` sweeps the full segment, including its endpoints, so a path can hit even if neither endpoint overlaps. Cover intercepts before the hull. A fitted part receives an impact only when it is present at the first hull contact; a later interior intersection cannot override that contact.

Both weapons apply damage through the same health update, clamped to the remaining hull or component health. Zero component health disables only its weapon. A destroyed hull cannot fire and still intercepts projectile segments with zero damage. Four projectiles can be pending; the fifth is rejected without eviction. The next tick reports one result per admitted projectile in admission order, including misses, cover, and zero-damage impacts. An accepted projectile still resolves when its shooter dies earlier that tick. The owner must drain accepted work before tearing down the fixture; there is no restart or save guarantee.

```sh
cmake --build --preset release && ctest --preset release --output-on-failure --no-tests=error
cmake --build --preset asan && ctest --preset asan --output-on-failure --no-tests=error
cmake --build --preset tsan && ctest --preset tsan --output-on-failure --no-tests=error
```

Linux x86-64, Clang 23.1.5/libstdc++, CMake 4.5.1, Ninja 1.13.2 and the existing Conan graph: all seven headless CTests passed in Release, strict ASan/UBSan/LSan and TSan. Combat ran 68 assertions in five cases. Range and launch assertions failed before implementation; the first-contact regression reproduced the review finding before the fix. Compile-time checks prohibit copying or moving the arena and its queue. Scoped format and clang-tidy passed. Independent follow-up source review found no remaining blocker. The fixture linked only standard runtimes. This is not moving-target sweep, multiplayer authority, wreck ownership, or acceptance of M5's playable loop.
