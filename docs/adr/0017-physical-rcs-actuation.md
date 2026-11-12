# 0017. Publish physical RCS firings from the flight simulation

Date: 2026-11-11

Status: Proposed; the owner explicitly authorized this scoped M3 implementation.
Full milestone acceptance remains pending verification and owner flight review.

## Context

The original controller applied a smooth net wrench to Jolt, then presentation
selected and blinked exhaust afterward. A dark plume could therefore accompany
continued physical thrust. The owner requires physical RCS, not an approximation.
The accepted mounts, main exhaust and handling targets remain constraints, but
exact historical trajectories cannot remain the acceptance oracle.

## Decision

The graphics-free flight simulation owns bounded RCS definitions and valve state.
Ship data supplies body-local force vectors and socket-pair positions relative to
the center of mass, plus calibrated signed-axis allocation. Both control modes
request a wrench through the existing controller. Actual firing levels produce
force and position-cross-force torque; only that physical wrench and the separate
forward main drive enter Jolt. The owning snapshot publishes those exact firings.
Rendering and audio observe actuation and never advance a second pulse clock.

Use a bounded, coordinated pulse gate driven by the fraction of configured
force/torque authority requested, not arbitrary nominal bank-rating units. Full
configured demand can remain on; lower demand uses bounded on-thrust compensation
and quiet intervals. Fresh opposing couples can interrupt old off debt. Bank,
force/torque and speed admission limits constrain the final physical firing levels.
This is fictional throttleable pulsed RCS, not a constant-thrust NASA valve model.

Keep 60 Hz stepping, staged command/reset semantics, native contacts, existing
speed and actuator bounds, and the main drive. Scale actual firing levels when
needed for speed admission; never assign velocity or publish un-applied burns.
The existing ideal continuous-actuator fixtures remain valid without an RCS rig.
Asset-specific calibration and material bindings stay in the flight application.
No renderer dependency, solver service or new GPU owner enters the game model.

## Consequences

Physical coasts now have no jet impulse, and arrest uses actual opposing burns.
Pulsing changes acceleration and trajectories, so current-source control, replay,
allocation, Windows/hardware and owner checks must replace historical motion-match
claims. The fictional pulse/thrust policy is not a NASA valve certification.
Definitions, reset and invalid admission need public tests. Snapshot consumers and
Windows structure alignment need affected native coverage.

## Alternatives Considered

- Keep post-physics visual pulses: rejected by the owner because they are not
  authoritative jet actuation.
- Script turn/coast/stop visuals: rejected; commands can exercise the controller,
  but only published physical firing states may select exhaust.
- Rebuild the Jolt backend or add a presentation physics owner: unnecessary; the
  existing force/torque boundary integrates summed physical jet wrenches.
