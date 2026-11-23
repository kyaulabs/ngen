# M5-B bounded mining yield

The offline game session owns one `mining::Site`: a full asteroid, an empty cargo hold, and a finite ore reserve. A claim transfers the least of its requested limit, the ore left in the asteroid, and the room in the hold. A full destination or empty source rejects the command without changing quantities. The game can explicitly issue `Replenish` to move ore from the reserve into free asteroid capacity; there is no clock-based refill or new ore creation.

One lock covers the operation ID lookup, reservation and immediate transfer, receipt, and snapshot reads. Concurrent callers cannot double-spend the same ore or cargo space; their arrival order is not guaranteed. Claims and replenishments share a 64-entry in-memory journal. An exact retry returns the first outcome, including a historical receipt or a rejection; changing the action or limit under an admitted ID conflicts. At capacity, new IDs fail without eviction. Replaying a rejected claim after replenishment does not mine newly available ore.

The contention fixture started with 5 ore in the asteroid and 7 in reserve. Two distinct 4-unit claims yielded 4 and 1 (12 units conserved); two copies of one 4-unit claim both returned the same receipt and left exactly 4 in cargo. The replenishment fixture started at `(asteroid 5, cargo 0, reserve 8)` and ended at `(3, 10, 0)`: all 13 units accounted for. Separate cases covered a full hold, depleted source, full journal and maximum per-reservoir counts.

```sh
cmake --build --preset release
ctest --preset release --output-on-failure --no-tests=error
cmake --build --preset asan
ctest --preset asan --output-on-failure --no-tests=error
cmake --build --preset tsan
ctest --preset tsan --output-on-failure --no-tests=error
ctest --preset tsan -R '^mining$' --repeat until-fail:20 --output-on-failure --no-tests=error
```

Linux x86-64, Clang 23.1.5 with libstdc++, CMake 4.5.1, Ninja 1.13.2 and the existing Conan graph: all six CTests passed in Release, strict ASan/UBSan/LSan and TSan. The mining target ran six cases with 105 assertions and passed 20 repeated TSan runs. Claim, replenishment and replay assertions failed before their implementations; TSan reported a data race before the lock was added. Scoped clang-format and clang-tidy passed. The fixture executable linked only standard runtime libraries.

This is a headless game fixture, not an installed SDK API. It does not connect to the market, schedule replenishment from simulation time, authorize players, persist across a restart, or accept M5's integrated playable loop.
