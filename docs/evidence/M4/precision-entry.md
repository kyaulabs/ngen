# M4 precision entry evidence

This is the entry investigation for [M4-A #11](https://github.com/kyaulabs/ngen/issues/11)
and [ADR 0003](../../adr/0003-hierarchical-coordinates.md), not production
coordinate or origin-transfer code. The entry probes use engine baseline `61693cd`, with no changes to `World`,
Jolt, flight, rendering or the SDK. The owner accepted the bounded M4-A
contract on 2026-11-18; this does not accept the later physical fixture.

## Arithmetic

`coordinate_precision.py` evaluates the proposed split representation in
binary64 against exact rational values. A one-axis model is sufficient to test
scalar cancellation and canonicalization; it does not test 3D consumers or a
native C++ API. Each anchor has a 0.3125 m separation and a 0.125 m local sample.
The flat control rounds each absolute point before subtracting it.

| Anchor | Split local position residual | Flat absolute control |
|---|---:|---:|
| 0 m | 0 m | 0 m |
| 1 AU | 0 m | 0 m |
| 1 ly | 0 m | 0.3125 m |
| Galactic, either sign | 0 m | 0.3125 m |

10,000 two-way arithmetic conversions across a cell boundary near the maximum
signed cell gave a maximum outbound position residual of **0.000238419 mm**,
return drift of **2.48689958e-11 mm**, and maximum velocity residual of
**4.26325641e-11 mm/s**. The analytic budgets are 1 mm and 0.1 mm/s. The probe
also checks canonical half-cell neighbors, signed carry/cell bounds, subnormals,
nonfinite input, both directions of the cross-cell difference, and mismatched
frame ID, epoch and tick. It checks bounds using Python integers; C++ must check
before narrowing or doing any signed arithmetic that could overflow.

Accepted M4-A scope:

- Signed 64-bit cells, width `2^32 m`; binary64 offsets in `[-2^31, 2^31) m`.
  Derive a checked integer carry and residual before resolving the half-cell
  tie. Do not add half a cell to an arbitrary double before splitting it:
  rounding there can carry the immediately preceding value into the next cell.
- Keep absolute and long-range positions split. Convert only nearby points to
  local binary64, checking cell adjacency **before** subtraction. For the
  first transform API, use an explicit component-wise bound of `2^20 m` for
  local input, origin displacement and result; reject larger differences
  instead of flattening them. This measured envelope is not a Jolt bound, a
  warp range, or a general millimetre guarantee over 1 ly of displacement.
- Transform nonrotating frames at one tick. Verify source frame ID and epoch,
  and require both frame samples at the pose tick. Convert both position and
  velocity; stamp the result with the destination frame. Reject nonfinite data,
  signed overflow and out-of-range conversions without mutating the inputs.
  C++ must rerun the analytic/negative cases with the implemented public types.

## Existing contact boundary

`contact_probe.cpp` drives the existing double-position Jolt `World` starting
at opposite local centers. A 10 m dynamic box starts with a 2 m face gap and
pushes into a static box at 6,000 N for 120 ticks at 60 Hz. The reported advance
is relative to the initial *center position at face touch*; it is not a shape
penetration metric or an agreed hull tolerance.

| Static box width | Local centers | Peak center advance | Terminal x speed |
|---|---|---:|---:|
| 10 m | −4,096 m and +4,096 m | 0.0416662729 m | 1.77323756e-9 m/s |
| 100 m | −4,096 m and +4,096 m | 0.0416662729 m | −1.33226763e-15 m/s |
| 1 km | −4,096 m and +4,096 m | Rejected at admission | Not run |

Release and strict ASan/UBSan produced the same printed results. The 1 km case
hits the **existing 256 m half-extent limit**, not a measured Jolt solver
failure. Keep that limit. A future representative 1 km compound/proxy and its
contacts must be tested as part of M4-B, not counted as passing here. Compare
translated samples to a no-transfer control and agree on contact thresholds
before the 10,000 live transfers. Do not use the analytic tolerances to infer
physical contact accuracy. No constraints, dynamic large-hull contacts, render
history, warp or runtime rebases were exercised.

## Reproduce

Run from the NGEN root. Results above used Linux x86-64, Python 3.14.9, Clang
23.1.4, CMake 4.4.5, Ninja 1.13.2, and locked
`joltphysics/5.6.0@ngen/headless-double#67a3b4c0d2e06dc7514a970066ac5074`.
Compute analysis and contact setup are independent of the M3 graphics and timing
fixtures. With the matching Conan toolchains already prepared:

```sh
python3 -B docs/evidence/M4/coordinate_precision.py

cmake -S docs/evidence/M4 -B build/m4-precision/contact -G Ninja \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/build/flight-release/conan_toolchain.cmake"
cmake --build build/m4-precision/contact --parallel 2
env -u DISPLAY -u WAYLAND_DISPLAY build/m4-precision/contact/contact_probe

cmake -S docs/evidence/M4 -B build/m4-precision/contact-sanitized -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/build/flight-asan/conan_toolchain.cmake"
cmake --build build/m4-precision/contact-sanitized --parallel 2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  env -u DISPLAY -u WAYLAND_DISPLAY build/m4-precision/contact-sanitized/contact_probe
```

The isolated CMake fixture compiles the existing world and core source with the
matched lock graph. It is not part of production CMake. The sanitized toolchain
used `-fsanitize=address,undefined`, non-recovering checks and its existing
non-PIE host configuration. No benchmark, Windows result or independent review
is claimed by this note.

## Native M4-A checks

The C++ implementation is in `engine/world/coordinates/`; `ngen_tests`
exercises its public API in `tests/coordinates.cpp`. The underlying CPU
library links no physics or graphics target. Six cases cover half-cell ties,
checked carries, local accuracy at AU/light-year/galactic locations, signed
cell-edge transforms, 10,000 analytic round trips, and input/frame rejection.
These are arithmetic operations, not physics updates. Compile-and-run Reds
confirmed the half-cell carry, unchanged sample across frames, and conflicting
state under an identical stamp were detected before each fix.

All four core CTests passed on x86-64 Linux with Clang 23.1.4 Release, GCC
16.2.1 Debug, and Clang Debug with strict ASan/UBSan and leaks. The first local
PIE sanitizer run aborted before `main` because the host mapping overlapped
ASan shadow memory; the existing non-PIE link approach (`-no-pie`) passed with
all detectors retained. Clang-tidy used actual Release compile commands for
the coordinate implementation and its tests. No hardware or host-wide claims
follow from these core checks.
