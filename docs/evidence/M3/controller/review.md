# Controller source review

Same-session Core/C++ review; not an independent reviewer or human approval.
Scope: `game/flight/flight.hpp/.cpp`, `tests/flight.cpp`, CMake/hosted consumers,
physics ownership/error contracts and the four actual native configurations.

Checked body/world force frames, fixed-step SI units, force-based limits,
co-moving mass/inertia, torque-only attitude, staged feasibility before native
mutation, immutable copies, command capacity/sequence/generation behavior,
replacement reset lifetime, off-owner rejection and failure publication. The
scalar speed limiter fixes the reproduced weak-axis counterexample without
inventing unavailable braking or assigning velocity. Navigation uses the same
actuator model; source contains no alternate kinematic integration path.

The owner-thread guard reads a lifetime-stable identity, not the pimpl that reset
replaces. Reset prepares before swapping and invalidates logical generations;
old snapshots remain owning data. Ordinary admission failures consume no queue
slot/sequence; an admitted but jointly infeasible batch consumes its sequences,
drops the complete batch and leaves native/published state unchanged. Native
failure is deliberately fail-closed, not falsely rolled back or published.

Four complete graphs pass24 selected/executed CTests, followed by four affected
final flight reruns after test lint repairs. Clang23's12 actual-command scope is
green after three affected test reruns. The initial AST findings, failed setup,
actual runtime Reds and invalid first heading-test frame assumption remain
separate. Format/whitespace and bounded headless dependency checks pass. Existing
GCC AST incompatibility is disclosed; no flags or analyzer checks are removed.

No remaining demonstrated P1/P2 defect was found in this headless slice. This is
not exhaustive physics/contact, concurrency, compiler or hardware verification.
The owner-thread-only contract does not permit concurrent snapshot reads or
object destruction. Cold allocation exhaustion is not a recoverable-memory
proof. Collision/profile integration and current Windows checks remain for the
playable slice; no milestone-completion or independent-approval claim follows.
