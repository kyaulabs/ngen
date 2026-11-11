# Shared headless flight controller

This checkpoint implements the shared fixed60Hz controller, not the playable M3
demo. `game/flight/` owns bounded next-tick copied commands and owning snapshots;
it has no window, renderer, audio, network or wall-clock dependency. Jolt math
stays behind engine/game contracts. M1's simulation/golden fixture is unchanged.

Direct thrust, explicit opposing-force braking and bounded angular-rate targets
share body-local force/torque limits with tactical destination/orbit control.
Release coasts; attitude release stabilizes through torque. Navigation never sets
pose or velocity. Destination steering turns the hull; orbit control reserves
force/angular-rate capacity and hull clearance. Candidate mode/navigation/mass is
validated together before native changes. An infeasible batch is dropped without
advancing; unexpected native failure closes until explicit reset.

Reset prepares a replacement physical domain, advances the logical entity
generation and drops queued/pilot state. Snapshots own their values. The Flight
is owner-thread-only; submit/step/reset reject off-owner calls before mutable
state access. The guard's owner identity remains stable when reset swaps its
private domain. Snapshot copying itself must happen on the owner thread.

Sixteen public controller cases cover direct response/coast, brake, attitude,
speed limits, destination arrival/heading, two120s orbital runs stable after30s,
co-moving mass response, atomic infeasible mass/orbit rejection, foreign-thread
recovery, anisotropic actuation, reset/stale generations, invalid/order/copy
admission,64-command exhaustion, mode changes during thrust/spin and same-build
fixed/variable presentation-cadence replay. Native body IDs differ between
replays and are not treated as portable recording identities.

Retained native text normalizes trailing whitespace; ignored originals remain
untouched. `red/` retains twelve actual runtime Reds. In particular, the first projected
speed-limit implementation required braking on an unavailable weak axis and
reached10.035511971m/s under a10m/s limit. The replacement solves for a scalar
wrench fraction: it preserves actuator feasibility and cannot add propulsive
energy above the speed envelope. It does not reset collision-induced overspeed.

The first heading Green attempt fails an incorrect world-axis/exact-float force
assertion, not the new heading behavior. The corrected test independently rotates
the world wrench using the pre-tick orientation and checks each body-local limit
with0.01N float tolerance. That attempted result remains explicitly failing.
An initial thread-test compilation used a nonexistent error enumerator; it is a
setup failure, not a Red. The real runtime Red is retained separately.

Four complete native graphs pass6/6 each: Clang debug/release/ASan+UBSan and GCC.
The final affected flight test is rebuilt/rerun in all four after lint-only test
repairs; strict ASan/UBSan/leak policy remains inherited from the preset. Both
headless ELF dependency graphs exclude SDL/Vulkan/display libraries. Twelve
actual-command Clang23 analyses cover physics/game implementations and both test
header consumers. Three initial test lint failures are retained separately from
three successful affected reruns. Typed array parameters replace a swappable
snapshot pair; malformed fixed-width enum data uses a C++20 object-representation
fixture rather than an out-of-domain cast. No check or warning is disabled.
GCC native builds/tests pass; GCC AST is not claimed because the unchanged Clang
frontend does not support the GCC module flags, which are not stripped.

The preceding ownership correction at `a8918ef` passes hosted CI4/4, including the
original Clang18 checks. Its metadata is retained as backend evidence, not proof
of this later controller source. This change extends hosted headless tests,
Clang18 analysis and dependency checks to the controller; its exact-head CI is
reported on the corresponding commit/run, not presumed here.

Cameras, contact preset, force/velocity plots, actual Windows compatibility,
playable accepted-asset presentation, bounded audio, offline packaging, warmed
hardware benchmarks, recording and owner control-feel feedback remain. Local
headless tests establish no GPU performance, packaged offline startup,
cross-compiler bitwise physics determinism or global heap/race-freedom claim.
