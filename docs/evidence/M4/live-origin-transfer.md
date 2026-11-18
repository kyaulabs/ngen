# M4-B live origin transfer

[Step 04 / issue #12](https://github.com/kyaulabs/ngen/issues/12) covers one
existing Jolt physics domain, translated in place between fixed ticks. On
2026-11-18 the owner approved 10 m and 100 m boxes, a two-section 1 km rigid hull,
and these fixture limits against a no-transfer control: identical contact
state each tick, at most **2 mm** of reconstructed position difference,
**2 mm** of constraint-length difference, and **2 mm/s** of linear velocity
vector difference. These are not general solver tolerances and do not replace
the separate [M4-A arithmetic limits](precision-entry.md).

`World::create` optionally takes a canonical split origin. `frame()` returns an
owning copy; `BodyState` carries its frame ID, epoch and tick. `rebase` checks
that the supplied stamp still names the live frame, and uses the M4-A transform
to resolve one translation without flattening a galactic position. Before
moving any bodies it checks every destination body origin against the existing
±8,192 m per-axis admission bound. A stale stamp, unchanged or invalid target,
out-of-bounds destination, or off-owner request leaves the domain unchanged.
Success advances the epoch, not the tick. It updates the existing Jolt bodies
and their broadphase bounds without changing rotation, activation, velocities,
accumulated wrenches or body-local constraint anchors. `in_contact` reports the
native pair result from the last physics step; it is not an overlap query.

The 1 km proxy joins two 500 × 100 × 100 m sections in one immutable Jolt
compound shape: one mass, one rigid body, two collision shapes. The original
single-box half-extent cap remains 256 m, the body cap remains 128, and the
solver still steps at 60 Hz. The fixture hits the compound's nose, not its seam.
The world owns up to 128 center-of-mass distance constraints and removes them
before destroying bodies. No native handles leave the adapter.

Each fixture has a 2 m initial nose gap to a static dock, a second dynamic body
on a distance constraint, and a 6,000 N forward force applied to the hull each
tick. Two native worlds step under identical inputs. The transfer world
alternates between split origins 4,096 m apart across adjacent positive X cells
at a galactic-scale location, once before each step; the control stays at the
initial origin. The test checks stamps, dock position and contact state on every
tick, and measures the largest Euclidean position and velocity difference of
either dynamic body, as well as the largest absolute difference in tether length.

| Hull | Live transfers | Matching ticks in dock contact | Peak position difference (m) | Peak velocity difference (m/s) | Peak tether-length difference (m) |
|---|---:|---:|---:|---:|---:|
| 10 m | 10,000 | 9,931 | 3.63407082e-11 | 0 | 2.95692359e-11 |
| 100 m | 10,000 | 9,931 | 2.39452902e-12 | 0 | 1.69819714e-12 |
| 1 km | 10,000 | 9,931 | 1.19611236e-09 | 2.44679932e-09 | 4.25075086e-10 |

Local Linux x86-64 results matched in Clang 23.1.4 Release, GCC 16.2.1 Debug,
and Clang 23.1.4 Debug with strict ASan/UBSan and leak detection (non-PIE
sanitizer executable). Each configuration passed all eight CTests, including
17 native physics cases. These builds used CMake 4.4.5, Ninja, C++20/libstdc++
and the existing `conan-physics.lock` double-position Jolt 5.6.0 recipe. The
four headless flight test executables had no graphics dependencies in their ELF
needed entries. The standalone M4-A screen still rejects the 1 km single box.

The first hosted CPU runs ([push](https://github.com/kyaulabs/ngen/actions/runs/44238733491),
[PR](https://github.com/kyaulabs/ngen/actions/runs/44238740123)) passed the
physical tests but failed clang-tidy 18's lifetime analysis across separate
local Jolt intrusive references to the same shapes. The revised construction
uses Jolt's `BodyCreationSettings` as the shape owner and its checked
`ConvertShapeSettings` interface for the compound. Local native results above
were repeated on the revision without disabling any analyzer or sanitizer.

After preparing the locked dependencies and configuring a `flight-*` build
with `NGEN_BUILD_PHYSICS=ON`, reproduce with:

```sh
cmake --build build/flight-release
ctest --test-dir build/flight-release --no-tests=error -V -R '^physics$'
build/flight-release/physics_tests '[rebase]'
```

The first test ran red when the epoch and positions did not change; the hull
test ran red when the 1 km nose did not reach the dock; the tether test ran red
when the other body did not accelerate and the separation grew; the origin
admission test ran red when a noncanonical origin was accepted. The native
physics suite also checks stale epoch and tick rejection, off-owner rejection,
atomic rejection when a later body would leave bounds, and accepted force
surviving a same-tick move.

This is a single axial docking scenario at each scale, with one active docking
pair and one tether. It does not establish behavior for arbitrary contact
networks, contact manifold or event identity, compound seams, high-speed
collisions, broadphase cost at saturation, or cross-platform determinism. It is
not the full B3 gate. Presentation/prediction history is M4-C; warp, streaming,
inter-world handoff, rotating frames and moving-origin velocity are not tested
or implemented here. [ADR 0004](../../adr/0004-local-physics-and-flight.md)
remains Proposed beyond the scoped evidence.
