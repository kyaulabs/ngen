# Initial M3 local physics backend

M2 completed through PR9/merge `4b554cc` and passing post-merge CI before this
slice. This is **not completed M3**, a flight controller, camera, audio or demo.
See the [implementation plan](../../../planning/m3-flight.md) for remaining work
and the owner's tactical-orbit/per-ship-first-person/coast-plus-brake decisions.

The optional headless graph uses Jolt5.6.0 via a separately identified static,
double-position CPU-only recipe. The stock recipe cannot select this ABI. The
same pinned upstream archive is unmodified; GPU/CPU-compute interfaces, examples,
debug renderer/profiler and object stream are disabled. Public defines, CRT and
baseline SIMD match the package. Preserve the upstream MIT notice; NGEN's own
license and private asset rights do not change.

The128-body RAII adapter has an owner thread, foreign/stale handle checks, finite
SI inputs, owning states and fixed60Hz stepping without a second worker pool.
Applied force/torque is checked against the native motion envelope before it can
be clipped; co-moving mass changes preserve velocity and recheck pending forces.
A native update-capacity error closes the domain instead of publishing success.
No global/system-OOM or arbitrary failure recovery guarantee is claimed.

Real public-interface Reds then Greens:

- Force6000N/mass1000kg for60steps: velocity0 failed expected6m/s; wiring native
  force/torque passes, with semi-implicit position3.05m and unchanged old state.
- Doubling mass after the first burn:12m/s failed expected9m/s; native mass/inertia
  scaling passes without resetting velocity.
- Unsupported impulses initially returned success; preflight rejects them before
  the native speed/spin clamp and preserves the next valid force.
- A mass reduction initially accepted an already excessive pending force;
  shared preflight now rejects it without changing mass or the accepted force.

At the initial `1189843` checkpoint, characterization also covers10s coasting/physical braking, actual static/dynamic
contact, torque/inertia, double-position precision, nonfinite input, foreign/stale
handles, off-owner access and capacity exhaustion. Ten physics cases pass. Exact
zero off-axis angular velocity was an overly strict test expectation: observed
float roundoff−1.6e-8rad/s is checked against1e-6. That correction is not an engine
bug or claimed behavioral Red.

Clang debug/release/ASan+UBSan and GCC each build and pass all5 selected CTests
(20total). Six actual-command Clang analyses cover the adapter and owned-header
consumer in the three Clang configurations. Three concrete lint findings were
fixed: use a named wrench instead of swappable same-type helper parameters,
wide scratch-size arithmetic, and lifetime-safe private member ordering without
excess padding. GCC native passes; unsupported GCC module flags are not stripped
for a fictitious clean clang-tidy result. Default M1/core regression tests pass.

Headless executable dependency inspection contains only standard runtime/math
libraries, no SDL/Vulkan/window dependency. The package consumer separately
registers Jolt and checks real local position precision. Startup/setup failures
are separate: prerequisite Jolt header ordering was corrected; a concurrent
Conan export race passed when serialized without cache deletion; PIE ASan shadow
startup collision passed with the existing host non-PIE link configuration.
ASan/UBSan/leak checks stay enabled; those failures are not behavioral Reds.

Hosted Clang18 subsequently diagnoses overlapping Jolt intrusive shape references
on the native body-allocation failure path, while native/ASan checks pass. Review
traces the factory cache/result/settings references rather than treating the
atomic decrement as an unconditional deletion. The adapter now constructs its
already-validated box directly into the settings' single local shape owner;
native success retains it and failure releases it. No analyzer, warning or
lifetime check is disabled. An additional public invalid-extent characterization
proves validation remains before construction. All11 physics cases pass in the
four native configurations and six Clang23 invocations pass. Exact-head
`a8918ef` [hosted CI42935201684](https://github.com/kyaulabs/ngen/actions/runs/42935201684)
passes4/4, including the original Clang18 check in Debug/Release/ASan. This closes
the hosted ownership diagnostic, not the remaining M3 demonstration gates.

Current scope does not establish Windows compatibility, actual selected-hull
contact fidelity, cross-platform whole-physics bitwise replay, M4 rebasing or
flight-demo acceptance. Controller/replay/plots, camera, renderer bridge, audio,
packaging, benchmarks, recording and owner feedback remain required.
