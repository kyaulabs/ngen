# M3 aft RCS visibility and coverage

The owner found the original aft jets hard to see and requested 16 forward jets
and 16 aft jets. This candidate has 16 paired banks: the accepted 16 fore/middle
jets, the relocated 8 aft jets and 8 additional aft jets. Main rear exhaust and
physical control behavior remain unchanged. The owner subsequently accepted every
location; the follow-on [pulsed firing work](../pulses/README.md) preserves this rig.

The exact rendered opaque LOD 2 mesh exposed the visibility defect: all 8 original
aft socket centerlines crossed engine housing geometry within 0.68–1.51 m. The
original hull-based mounts were inside the drive fairings. A failing regression
retains those eight obstructions; the new derivative passes all 32 socket-path
checks. This tests exhaust centerlines, not visibility from every camera angle or
the full plume volume.

The aft banks now sit on the authored outer side-drive fairing facets at the
113 m and 141 m longitudinal stations. The extra hardware uses the existing
8-sided sockets/materials and unchanged plume geometry/style. Six added hardware
sets are baked into three material-grouped draws, with inverse-transpose normals
and transformed orthogonal tangents. No renderer pass, resource owner, dependency,
shader change or material ABI change is introduced.

All original M2 sections/prefixes remain exact. Relative to the first RCS package,
vertex/index/primitive/material/string prefixes remain exact; only 10 existing
instance records belonging to the four aft banks move. All eight fore/middle
bank transforms remain exact. The additional banks are appended. Calibration
averages feasible nonnegative allocations so every bank participates in valid
wrench patterns instead of permanently favoring only the farthest pair.
Calibration remains outside the frame loop; no new physical forces are applied.

Private candidate: `build/cooked-candidate/frigate-rcs-aft-v3/scene.ngas`.
SHA-256: `90e55636228021f6bd04e800d2884883d6a3418eef716b396a28640e2e5270d2`.
CRC compatibility marker: `65ddab97`, not authentication. The workload is
249,942 triangles with 247 ship instances and 33 materials. Existing limits are
unchanged; only 58 triangles remain in the current workload budget.

Verified with Clang Debug/Release, strict ASan+UBSan and GCC:

- Four complete 21-test graphs pass; scene tests now contain 17 cases.
- Fifteen complete actual-command Clang analyses pass for all five affected
  implementation/header consumers in three configurations. Native GCC passes;
  unsupported GCC AST flags remain unchanged and AST coverage is unavailable.
- Twelve opt-in native rig/replay/preservation cases pass, plus four offline
  authoring failure cases. The late manifest collision specifically reaches
  exclusive publication and preserves the owner file.
- The 2,400-frame 1920 × 1080 Release replay has all GPU rows, empty stderr and
  0 C++/0 ImGui warm allocations. All 24 physical/rear-power fields match the
  original-asset baseline exactly. This muted dummy-backend run is not listening,
  Windows memory or final three-run timing acceptance.
- Matched private native before/after upper/lower aft closeups show the exposed
  nozzles and added pairs. These all-on asset views are not physical flight traces;
  private images and package bytes are not committed.

Run `tests/flight_rcs.py` with the native demo/private candidate/base environment,
`tests/rcs_author.py` with the accepted base, and `tests/rcs_visibility.py` with the
private candidate. The visibility regression uses the existing NumPy authoring
environment, not a new native/runtime dependency. `checks.json` binds results to
actual source hashes. Same-session review covers placement, baking, ownership,
calibration, original preservation and capacity; it is not independent review.

The private Linux v9 bundle is `build/m3-offline-linux-v9/ngen-flight`, built
from signed behavior source `c63c4520430f1e50ff93f6b45b112a5eb7cf5881`. All 23
manifest hashes and its bounded 241-member source archive validate. Headless rig
verification and three rendered frames pass with networking unshared, Conan hidden
and SDK/library-path overrides absent. See `offline.json`; platform ABI and final
UI/Windows/budget qualifications still apply.

Previous RCS behavior source `464d3fd` and evidence source `bc65a1b` hosted runs
43136111167 and 43136920511 pass all four jobs. New-source hosted CI, owner review,
current Windows/RTX checks and the recorded CIM/ImGui findings remain separate
requirements. The deferred EVE-inspired UI pass and final affected delivery,
budgets, review and normal integration are still required before M3 completion.
