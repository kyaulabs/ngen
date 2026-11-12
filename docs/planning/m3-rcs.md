# M3 RCS actuation visuals

The owner confirms the rear exhaust works and requests visible side jets for
turning, rolling and strafing. The existing model source has 6 upper-side banks
with 12 sockets. The owner chose matching lower opposing banks: 12 banks and
24 sockets in total. Implement this before the deferred UI refinement.

Preserve the accepted rear exhaust, original hull/material/image/light data and
physical controller. Create a new private derivative; never overwrite the accepted
package or original authoring inputs. Reuse the existing cooked-section assembler
and authored append-only derivative pattern. No full recook, vendor-source bypass,
new renderer pass/backend/resource owner, dependency or material ABI is needed.
New hardware and plumes must stay inside the existing 250,000-triangle workload,
256-instance admission and local bounds. Do not relax those limits.

Place lower banks on matching ventral side facets using the original hull stations
and verified exported scale. Reuse existing opaque materials. Add small blue-white
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

Finish affected Linux/Windows, packaging and performance checks before the UI pass.
The latest ClangCl baseline builds and validates both configurations, but its first
CIM query failed with a general provider error and its clean replay reports 2 warm
ImGui allocations. Both findings stay open; no samples, counters or limits are
exempted. Preserve those raw results when collecting final evidence.
