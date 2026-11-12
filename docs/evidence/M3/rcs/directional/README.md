# M3 directional RCS couples

The owner corrected the all-around firing and authorized re-aiming nozzle pairs,
not moving the 32 approved mounts or changing the accepted flight response.
The earlier pulse scheduler only controlled timing. Its calibration averaged
redundant feasible allocations to make every bank participate, including banks
producing opposing moments; the inspection held synthetic torque continuously.
Neither demonstrated the requested turn → coast → opposing-stop behavior.

This correction keeps the pulse scheduler and replaces that allocation policy.
Offline minimum-total-impulse selection uses the actual nozzle directions and
moment arms for each signed wrench axis. Pure-axis requests cannot select a bank
that opposes that axis. Zero-component helpers can cancel incidental force/moment.
Pitch/yaw use three paired banks and roll uses two; the corresponding arrest sets
are disjoint. There is no forced participation, random blink or runtime solver.
Combined-axis requests still superpose their calibrated demands; this is not a
claim of globally optimal propellant use or a certified valve/impulse simulation.

The private candidate is
`build/cooked-candidate/frigate-rcs-directional-v1/scene.ngas`:
SHA-256 `99c996f33d3cc98dbd801e3aa302ee2a5894d0fa6bcfdd18f59b73e72e74d834`,
CRC compatibility marker `6bbe6157`. It retains 247 ship instances, 33 materials,
32 sockets and 249,942 workload triangles under unchanged limits.

Mount plates and all 32 socket-body pivots stay in place. Sleeves, throats and
plumes rotate together about those pivots. The two braking banks cant bowward
exhaust outboard so one socket cannot fire through its partner. Only 2,204 original
upper fore/middle socket vertices change; an independent old-rig whitelist proves
all other original vertex bytes unchanged. UVs/handedness, original indices,
primitive/instance/material prefixes, textures, lights and accepted main exhaust
remain exact. Asset provenance records the re-aim and calibration without changing
original authoring inputs or historical packages.

The sparse/disjoint public regression fails the old calibration with 16 active
banks against the six-bank bound, then passes. Actual three-axis controller tests
retain quiet RCS while angular velocity exceeds 0.35 rad/s, then light a disjoint
arrest set on release. Native geometric verification independently reconstructs
all bank forces/moments from immutable transforms and rejects counteracting pure-axis
contributions. Exact rendered-mesh checks verify all 32 exhaust centerlines,
unchanged socket pivots and the original-vertex whitelist. This is centerline,
not full plume-volume or every-camera visibility proof.

Four complete Linux graphs pass 22 CTests each, followed by the two affected test
reruns per graph. Twelve actual Clang command analyses cover the changed source
and calibration consumers in Debug/Release/ASan. Twelve opt-in native RCS cases,
four author process/publication boundary cases and three exact-geometry cases pass.
A Catch flag-enum analyzer diagnostic is avoided with an equivalent positive
assertion, not suppression. Initial test-macro/header transcription compilation
failures are retained separately and are not behavioral Reds. GCC native checks
pass; its AST flag incompatibility is not bypassed.

The 2,400-frame 1920 × 1080 replay retains all 24 original physical/main-drive
fields exactly, finite HDR, all GPU rows, monotonic host clocks, empty stderr and
0 C++/0 ImGui warm allocations. The CSV remains 63 columns within the existing
64-column observer limit. Muted dummy audio is not listening, final timing or
Windows memory acceptance. `checks.json` binds results to source hashes.

Same-session review checked the re-aim transforms, socket pivots, normal/tangent
rotation, complete-face selection, sparse allocation, main-drive exclusion,
geometry preservation, capacity and exclusive publication. No further actionable
finding remains in this slice; this is not independent review.

The new owner-closed, fixed-hull inspection uses actual controller snapshots and
preceding attitude to show spin-up, quiet rotation and opposing arrest. It cycles
physical maneuvers rather than holding synthetic torque. The hull is fixed only
for viewing the emitters; this is not gameplay or performance evidence. No human
client has automatic termination, recording or microphone capture.

Owner re-aim/inspection approval, current Windows/RTX and historical CIM/ImGui
findings remain open. Deferred UI refinement, final affected delivery/budgets,
review and normal integration still gate M3 completion.
