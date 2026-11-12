# M3 RCS implementation checkpoint

The owner approved matching lower opposing banks in addition to the existing
upper sockets. The candidate now has 12 banks and 24 blue-white socket plumes.
Only the application owns the rig/mixing policy; no engine, shader, renderer pass,
GPU owner or 64-byte material ABI changed. The accepted rear exhaust mapping is
unchanged.

The private derivative is `build/cooked-candidate/frigate-rcs-v1/scene.ngas`:

- SHA-256: `bc42a7935562bdfde872b2d09dbb473996cdbcddc5101418bca2f632ca5dc427`.
- CRC compatibility marker: `423256f5`; not authentication.
- 194,132,322 bytes, 240 ship instances, 29 materials and 248,934 workload triangles.
- Original geometry/material/instance/string prefixes are exact. Images, textures,
  samplers and lights are exact; the original package is untouched.

`apps/flight_demo/author_rcs.py` reuses the existing assembler and the already-used
NumPy 2.5.4 offline authoring environment. It appends lower hardware and small
crossed-card plumes; it does not recook vendor inputs. Exhaust starts at the
verified authored socket mouths. Calibration uses actual force directions and
moment arms, with finite nonnegative weights computed outside the frame loop.
NumPy is not a new native/runtime/startup dependency. Rebuilding with changed
source/provenance requires a reviewed package/profile pin, not a silent fallback.

The application converts published force/torque using the preceding tick's
attitude. Per-bank levels are bounded and copied; stabilization fires reverse
pairs after stick release. Zero actuation extinguishes RCS. Forward drive remains
on the accepted rear plume. RCS radiance uses a fourth-root response so small
translation demands remain visible; it never changes physical force or motion.
The CSV adds `rcs_active` and `rcs_power`, staying within the unchanged 64-column
Windows observer limit at 62 columns.

Public runtime Reds covered missing complete bindings, invalid/duplicate/unused
banks, no response to lateral force, and no response to turning/stabilization.
All are Green. Characterization covers idle/rear-only thrust, reversed axes,
rotated interval attitude, bounded mixed actuation and invalid finite/unit inputs.
The native `--verify` also reconstructs force/moment axes from actual package
transforms and compiled mixing data, independently of authoring JSON.

Verified on local Clang 23.1.4 Debug/Release, strict ASan+UBSan and GCC 16.2.1:

- Four complete 21-test graphs pass, followed by four affected scene-test reruns.
  The scene executable now has 16 cases.
- Fifteen actual-command Clang analyses cover all five affected implementation/
  header consumers in three configurations. Three test-only diagnostics were
  repaired without suppression; their three affected reruns pass. GCC AST remains
  unavailable because its unsupported flags are not removed; native GCC passes.
- Twelve opt-in native cases pass: geometric rig verification, validated replay
  and exact frozen-asset comparison in all four configurations.
- Four offline authoring failure cases preserve owner output, reject an unknown
  base, roll back a failed probe and preserve a late owner manifest collision.
- A clean 2,400-frame 1920 × 1080 Release replay has all GPU rows, finite samples,
  monotonic host clocks, empty stderr and 0 C++/0 ImGui warm allocations. All 24
  physical/rear-power trace fields match the original-asset baseline exactly.
  This muted/dummy-backend run is correctness/allocation evidence, not listening,
  three-run timing, process-memory or Windows GPU-memory acceptance.
- Private native idle/hot upper and hot lower closeups show the paired jets and
  matching lower hardware. The same-camera idle/hot comparison changes 9,497
  pixels by more than 8 codes. These all-emitter lookdev views are not physical
  flight traces. No private images or package bytes are committed.

Reproduce the opt-in checks with `NGEN_FLIGHT_DEMO`, `NGEN_FLIGHT_PACKAGE` and
`NGEN_RCS_BASE`, then run `python tests/flight_rcs.py` and
`python tests/rcs_author.py`. Ordinary CMake/CTest runs cover the public CPU cases.
`checks.json` binds these results to source hashes and records the remaining scope.

Same-session review traced borrowed material tables, copied levels, finite/error
admission, profile padding, actual bank moment arms, original asset preservation,
exclusive publication/rollback and warm-loop work. No additional actionable defect
was found; this is not independent review or final M3 acceptance.

Owner RCS appearance review, current-source Windows/RTX acceptance, the historical
ClangCl warm ImGui allocation finding and failed CIM sampling remain open. The
required EVE-inspired UI pass still comes after non-UI technical work and before
M3 completion. Final affected bundles, budgets, review and normal PR integration
remain required.
