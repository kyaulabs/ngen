# M3 RCS actuation visuals

The owner confirms the rear exhaust works and requests visible maneuvering jets
for turning, rolling and strafing. The initial 24-jet look was approved, but the
aft jets were hard to see. The owner now requests 16 forward and 16 aft jets:
16 paired banks and 32 sockets. Preserve the forward look, expose the existing
aft banks on the outer drive fairings and add four aft pairs. Implement this
before the deferred UI refinement; current evidence is in the
[aft RCS checkpoint](../evidence/M3/rcs/aft/README.md).

Preserve the accepted rear exhaust, original hull/material/image/light data and
physical controller. Create a new private derivative; never overwrite the accepted
package or original authoring inputs. Reuse the existing cooked-section assembler
and authored append-only derivative pattern. No full recook, vendor-source bypass,
new renderer pass/backend/resource owner, dependency or material ABI is needed.
New hardware and plumes must stay inside the existing 250,000-triangle workload,
256-instance admission and local bounds. Do not relax those limits.

Use actual outer fairing surfaces for aft mounts, not the enclosed pressure hull.
Check all exhaust centerlines against rendered opaque LOD 2 geometry. Preserve
existing fore/middle mounts and use the verified exported scale. Reuse existing opaque materials. Add small blue-white
plumes at all sockets using the accepted drive texture/style and existing blend
and emissive-flow path. The new geometry is original; no EVE assets are involved.
Existing package sections/prefixes remain exact except for appended records and
updated provenance/checksum. Runtime loading must validate the finished derivative.

Keep the visual rig in `apps/flight_demo`. Bank transforms and material bindings
must correspond to actual authored sockets. Author bounded per-axis mixing data
from bank force directions and moment arms; keep calibration outside the measured
loop. Use actual published world force/torque and the preceding tick's attitude,
not keys, mode labels or velocity guesses. Opposite torque reverses the jet pairs;
stabilization can fire jets after pointer release. Zero actuation extinguishes
RCS plumes. Rear-drive mapping and physical pose/motion remain unchanged.

Use public-boundary TDD for material/bank admission, finite bounded actuation,
force direction, opposing moment pairs, reversed axes and rotated interval attitude.
Test malformed/incomplete/duplicate bindings, invalid values and independent copied
results. Verify new package provenance and preservation of original geometry,
rear plume, materials, images and lights; check triangle/instance/memory limits.
Native renders must show idle, strafe, pitch/yaw/roll and arresting jets in useful
views. Keep captures private and get owner feedback on the added RCS only.

The owner accepts all 32 locations and requests pulsed RCS rather than constant
low-demand exhaust. Preserve every approved mount and the private asset pin.
Conventional valved RCS uses on/off control, deadbands/hysteresis to limit chatter,
and maneuver/coast/opposing-stop firings; see Hall et al., *Design and Stability of
an On-Orbit Attitude Control System Using Reaction Control Thrusters* (NASA, 2016),
[sections II, IV–VI](https://ntrs.nasa.gov/citations/20160001829). NASA also identifies
minimum impulse bit as a control-precision constraint in its
[GNC survey, section 5.2.4](https://www.nasa.gov/smallsat-institute/sst-soa/guidance-navigation-and-control/).
These sources do not specify numerical timings for NGEN's fictional ship.

Implement a bounded application-owned presentation pulse budget, advanced once
per successful 60 Hz tick, never per rendered frame or wall-clock timer. Gate the
whole calibrated wrench pattern together so moment pairs remain coordinated.
Keep the existing on-burst appearance and derive visual duty from the existing
perceptual response. Use an idle deadband with hysteresis, a 6-tick minimum burst
and a 3-tick minimum off interval; saturated demand can remain on continuously.
Those 100 ms/50 ms values are presentation choices, not measured valve limits or
a certified impulse model. Minimum intervals apply while correction demand
persists; idle/reset cancels promptly and saturated demand can respond immediately.
Reset clears stale pulse credit. Invalid values reject atomically. The physical controller, applied
wrench, audio policy, accepted rear exhaust and 64-byte material ABI do not change.

Test low-demand puffs, quiet/coast and hysteresis, sustained saturation, changes
from strong demand, invalid/reset state, owning results and render-cadence
independence. Preserve demanded versus displayed actuation in the bounded CSV.
The all-on coverage view remains a deliberately nonphysical location inspection,
not evidence of normal firing behavior. Show the owner a separate pulsed preview.

The owner then corrected the simultaneous all-around firing and authorized re-aiming
nozzle pairs while keeping all 32 mount locations and the accepted flight response.
The previous averaging policy deliberately lit redundant/opposing banks, and its
constant-torque inspection fixture did not demonstrate a physical turn/coast/stop.
Remove that policy. Author sparse minimum-total-impulse allocations per signed
axis; no active bank may counteract a requested pure axis. Pure turn and arrest
sets must be disjoint. Zero-component helpers may cancel incidental force/moment.
Forward thrust remains exclusive to the main drive.

Preserve all socket-body pivots and mount plates. Re-aim the sleeves, throats and
plumes together; change original vertices only at the four upper fore/middle
socket pairs, with an independent exact whitelist check. All other original
vertex bytes, UVs/handedness, indices, primitive/instance/material prefixes,
textures, lights and the main drive stay unchanged. Keep the workload limits.
Verify all 32 actual rendered exhaust centerlines after re-aiming, not assumed
local socket offsets. Show actual controller spin-up, quiet rotation and opposing
arrest, not a permanently held synthetic torque. This corrects the implementation;
it does not certify a propellant/valve model or change physical control behavior.

The owner now explicitly chooses **physical RCS actuation**, not post-physics
visual pulses. [ADR 0017](../adr/0017-physical-rcs-actuation.md) records the boundary:
controller → allocation/valves → physical jet wrench → Jolt → owning snapshot →
rendering. Retain mount geometry and handling targets; exact old trajectories are
historical, not an oracle for the changed physical model.

Current implementation slices:
1. Add owning physical bank definitions and published firings. Public Red must show
   the old smooth wrench without matching burns; Green reconstructs actual force
   and moment from the firings supplied to Jolt.
2. Move valve state into the graphics-free simulation and remove the application
   pulse clock. Render only published firing/main-drive states. Verify true quiet
   coast, opposing arrest, rotated/mixed actuation, reset, bounded admission and
   speed-limited firing consistency.
3. Recheck current-source handling, contacts, tactical navigation, all affected
   consumers/configurations, native replay/allocation and private packaging. Show
   owner-controlled physical gameplay, not a fixed-hull staged visual sequence.

These slices are implemented and verified locally: eight complete graphs,
60 actual Clang command analyses, 16 pulse/physical cases, native RCS/SDL checks
and a full-HD allocation replay pass. The [physical checkpoint](../evidence/M3/rcs/physical/README.md)
records real authority and supersedes the post-physics motion-match claims.
Owner appearance/physical flight acceptance remains pending. Preserve historical
pulse-only and direction-only evidence with explicit superseding links.

Finish affected Linux/Windows, packaging and performance checks before the UI pass.
The latest ClangCl baseline builds and validates both configurations, but its first
CIM query failed with a general provider error and its clean replay reports 2 warm
ImGui allocations. Both findings stay open; no samples, counters or limits are
exempted. Preserve those raw results when collecting final evidence.
