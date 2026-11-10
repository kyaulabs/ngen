# M2 exhaust motion

The owner identified that the accepted blue-white plume is stationary and
requested fluid-looking exhaust ejecting aft. The larger radius, longer tail and
inspector/input remain accepted. Later reference-guided decisions below supersede
the earlier white-base/blue-tail color distribution, not the original files. The first actual moving preview
ran 3,037 queued frames over about one minute, with validation on and empty stderr.
The owner clarified that the motion looked good but the tail ends like a knife;
the same close-up exposed flat-looking hull surfaces. Target a worn industrial
metal finish, as explicitly selected: keep the gray/red scheme and geometry,
add restrained surface wear and useful roughness/normal variation, not rust or
mirror polish. PR9 stays draft; M3 does not start before these M2 gates close.

Latest owner decisions: the analytical soft tail fade is accepted. The hull is
still unaccepted after alloy and panelled candidates. The owner supplied two
additional hull references and a jet reference, all private and guidance-only;
do not copy reference pixels, logos or assets. The 48 Hz softened sine field was
rejected as too uniform. Replace periodic bands with rapid irregular longitudinal
filaments, independent engine turbulence and a long continuous visual cycle.
The owner explicitly selected “Match reference core”: extend the white-hot center
along most of the jet with blue fringes and a ragged fading tip. The owner then
called that turbulent version “WAY better” but explicitly reopened shape: narrow
it, originate at the recessed white emitter/inner blue coil rather than the outer
bell, extend it farther aft and taper as a droplet almost to a point. Preserve
that improved flow/color style and dark housings. Focus on the jet before asking
for another hull verdict.

The new asset measures each actual emitter face and inner-coil opening from the
cooked model. Roots sit about0.02m beyond the coil face (x149.72 rather than159.5),
inside its opening. Only the owned appended jet mesh/transforms change; the fake
base disc is removed. A rounded16-ring cross-card profile narrows to about0.5%
of maximum radius and doubles the old exposed tail behind each bell. The actual
workload becomes247,520 visible triangles, below the unchanged250,000 cap. Native
admission, recessed-root/width/length/taper checks and HD rendering pass; owner
acceptance and final broad checks were still open at that stage. The owner has
now explicitly selected **“Accept exhaust”** for the corrected recessed/narrower/
longer pointed-droplet package9b645de103ea92677c3cfe97cb8bd88ede8e7109f34c0f926888ba65c7522e63
with the seeded irregular-flow shader. Freeze this appearance/motion; do not ask
for that verdict again. Hull appearance, full verification and normal integration
remain open. A first authoring attempt retained
the smaller primitive bound and was correctly rejected; its corrected bound is
computed from the actual profile, not waived.

The hull-only preview completed after the exhaust verdict. The owner identified
an excessively bright cyan patch on the command citadel. It is surrounding hull
lit by six bridge-window proxies, not a reactor or one giant window: the actual
`F01_Window` glazing is six narrow strips. Its original proxy lights sit outside
the surface with 9 m reach. The proposed bridge-v2 derivative dims only glazing
emission, moves those six proxies into the pressure compartment and limits reach
to 0.6 m. It preserves their powers and the selected 16-light set, so unrelated
lamps do not become active. A lower-power first candidate fixed the cyan wash but
activated a roof-lamp hotspot; it is retained, not the proposed final candidate.
The native adjacent-hull patch changes from 6,600/6,600 nearly clipped cyan pixels
to zero. Independent byte-scope checks and byte-identical aft/quarter/rear captures
confirm that the bridge fix does not change the accepted exhaust. The completed
bridge-v2 preview queued 4,860 frames with validation and empty stderr. The owner
still rejected the hull as too light gray and the glazing as a cyan dashed line;
it must look transparent, translucent or glass-like. Neither is accepted.

The next material-only candidate darkens the three coating base colors by 0.4 in
linear space and replaces window emission with dark smoky dielectric glazing
(metallic 0, roughness 0.06, alpha 0.28) through the existing transparent PBR path.
It preserves the six original window meshes, bridge-light correction, all images,
all other materials and accepted exhaust. The source has solid hull behind these
small strips, not a modeled cockpit interior; do not claim new interior visibility
or refraction. Native admission, rendering and absence-of-spill checks pass. Its
preview completed 5,020 queued frames with validation and empty stderr. The owner
explicitly said **“Hull is good”**: freeze the charcoal coating, panel/wear/normal/
ORM maps and forward cladding shown in package
`27de456e8f29b6392a50099dd65f69b2bd42a8ce2a73b3b3b0cc2c716811ee81`.
This accepts the hull finish only, not the complete package or M2. The cockpit
panes still look like solid metal because solid hull backs the existing strips;
glazing remains unaccepted. Visible interior depth requires a separate localized
geometry decision, not another blind material adjustment.

The owner then explicitly authorized **“Recess windows”**: cut openings behind
those six windows and add shallow dark backing, without a full cockpit model.
The new derivative cuts only local patches of the raked/chamfered citadel surface
at all three LODs, replaces the raised solid boxes with single translucent panes
following that surface, and adds black frames and 0.9 m-deep recesses. Existing
bridge-light proxies move deeper inside so they cannot wash the visible backing.
Accepted material/map data and every unrelated primitive's attributes/topology
remain unchanged. A public-package sightline check fails all 18 window/LOD cases
before the cut and passes afterward. Native admission/rendering, ASan/UBSan
admission, nondegenerate-cut/scope checks and the no-cyan-wash control pass.
All three aft captures remain byte-identical. The workload is 247,542 triangles,
still below 250,000. Candidate
`a02d5438ad32d9c8fb917ce847d5a822688ca5b51d69125f6da71cf2ba2116ed`
was shown in a bridge-only, 5,154-frame validated orbit with empty stderr. The
owner explicitly selected **“Accept bridge windows”**. All current visual gates
are accepted; freeze their appearance and complete relevant verification,
evidence, review and normal integration. M2 is not closed by visual approval
alone; M3 remains gated. Do not reopen accepted hull/exhaust/bridge verdicts.

Local source verification now covers Debug, Release, ASan/UBSan and GCC: 12/12
CTest cases per graph and eight successful native resource/inspector runs with
empty stderr. Shipping, shader-disabled and TSan graphs also pass. Two subsequent
C++ lint fixes were checked with affected builds, tests and native runs. Clang
analysis passes 111 actual compile invocations, including affected header consumers;
GCC's native graph passes, but clang-tidy cannot consume its actual module flags.
Native flow also clamps the normalized guide before lattice integer conversion;
its boundary-equivalence Red/Green and the deliberate phase-jump mutation remain
retained. Clean final-package timing, scoped Windows motion checks, evidence
retention, review and normal PR integration are still open.

Retain the completed Windows evidence: source5a2f841 passes10/10 tests/new bounds
controls, and its reused Release binary executes accepted package067407fb… with
exit0/empty validation stderr/600 frames/four reloads/two resizes. Those results
prove the static presentation, not this new animated renderer behavior. Do not
repeat the completed unrelated compiler matrix or memory observation.

Use the existing transparent PBR path, Context ownership and immutable package.
No fluid solver, particle framework, new pass/resource owner, dependency, added
point-light definition, original-file mutation or package-schema change. A separate private hull
texture/material derivative is now explicitly authorized; preserve the original
GLB, canonical package and earlier accepted derivative. Keep GPU material
size64 and existing bindings/layouts: define the reserved fourth material word
as packed normalized phase(low16) and amount(high16). Zero disables motion;
phase-only words are rejected. The high16 bits of the third material word carry
a stable flow seed only when flow is enabled; its other reserved bits stay zero.
SceneRenderer derives the seed from source-node identity, not world/camera
position, so engines differ without swimming as transforms change. Nonzero flow
is supported only for blend draws. These are explicit GPU contracts, not
repurposed camera/light padding.

Expose a borrowed complete material-indexed flow table on SceneView, matching
the existing factor-table pattern. Each entry is finite phase[0,1) and amount[0,1].
Validate the whole table before submission, then copy encoded values into the
existing per-draw material constants. Caller storage is not retained; old versions,
frame pins and successful-submit publication follow the unchanged owner path.
Empty/zero flow preserves authored static rendering and all unrelated materials.

The M2 workload binds motion explicitly to its authored presentation material,
not by name inside the renderer. Resolve that fixture binding only at package
admission/replacement boundaries. A bounded monotonic visual-clock phase drives
it independently of simulation state; this is not M3 thrust/flight behavior.
Record enabled/bound material and clock scope in evidence. The new profile wraps
a normalized phase over16 seconds, advects512 periodic lattice cells per cycle
(32cells/s; three cells along the authored plume), and samples a trailing0.6-cell
window in four taps (18.75ms). Irregular anisotropic noise replaces sine harmonics;
its long wrap must be continuous. Generic assets remain static unless the caller
supplies flow controls.

Advect smooth multiscale density toward increasing UV0.u, the authored aft axis.
Keep the hot-base region unchanged, use a smooth transition into the tail, and
perturb tail density/emission/alpha and transverse sampling modestly. Enforce a
smooth aft fade independently of texture LOD: coarse/oblique mips average nonzero
alpha at the finite card edge. The constant-white-texture boundary regression
failed genuinely, then passes with an analytical tip envelope; core, aft-advection,
repeat/wrap and copied scene-flow controls remain green. Keep exposure1/FXAAon/
bloom.04 defaults. The pattern
must be continuous across phase wrap; it must look like spatial flow, not whole-
plume blinking or movement of the metal housing. No physical propellant velocity
or CFD accuracy is claimed.

Implement one behavior per native Red/Green cycle:

1. Admit supported packed blend flow and reject unsupported controls through the
   production mesh interface; retain default material/layout contracts.
2. Fixed-camera HDR readbacks must change in the tail between phases, preserve
   the base/unmarked material, and demonstrate aft translation of an identifiable
   pattern rather than merely different pixels. Test wrap continuity and repeat
   phase consistency without copying the shader's noise implementation.
3. Scene material-index admission/ranges and copied asynchronous frame state;
   workload binding survives completed reload/reset without changing source or
   old snapshots. No new warmed frame-preparation allocation.
4. Run relevant native/compiler/layout/reflection/sanitizer checks and clean
   laptop timing on the accepted package. Use actual moving preview/capture,
   not a still image as evidence of motion. Arrange the visible run only when
   ready, then launch immediately after the explicit cue.

A new shader/CPU contract needs scoped Windows/RTX verification once the local
change is ready. Previous static evidence is retained, not relabeled as motion
verification, and no completed unrelated test is requested again. Close the new
motion/surface requirements and normal CI/PR integration before calling M2 complete.

The owner separately authorized committing the supplied brand pack. All 119 files
(6,361,908 bytes) are retained without byte changes in signed commit d036c91 on the
M2 acceptance branch. The PDF is classified binary, not whitespace-rewritten.
This does not authorize an unrelated app/README reskin or close rendering gates.
