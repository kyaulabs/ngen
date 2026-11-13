# M3 offline flight implementation

M2 delivery completed before this work: [PR#9](https://github.com/kyaulabs/ngen/pull/9)
merged the verified tree at `4b554cc`; post-merge
[CI42920708365](https://github.com/kyaulabs/ngen/actions/runs/42920708365) passes4/4.
The completed branch and clean review worktrees are removed. Current M3 branch
starts from that exact integration. M3 is **not complete or accepted**;
execute the five [milestone outcomes](milestones.md#M3--ship-the-polished-offline-flight-demonstration).

Owner decisions: tactical orbit is the default. Direct control switches to a
first-person view at the active ship's authored flight-camera mount; different
ships must be able to supply different local mount transforms. Releasing thrust
coasts; explicit braking applies bounded opposing force. Switching modes does
not reset pose or momentum. No additional M2 art approval or cockpit remodeling
is implied; preserve the accepted asset/material/effect parameters.

Keep one local flight scene, one controlled hull and100instanced asteroids. No
login/network/SQL/Lua/galaxy/warp/distributed authority path enters this milestone.
Reuse the M1 clocks/IDs/results/command principles and M2 Context-owned rendering,
immutable packages/snapshots, controls and offline shader path. Preserve the
existing M1 golden fixture rather than replacing its one-dimensional contract.

1. **M3-A: headless physics and force/torque model.** Resolve and pin the proposed
   Jolt5.6.0 double-position CPU-only dependency, verify its real recipe options
   and headless link graph, then add a bounded RAII adapter in `engine/physics/`.
   No second worker pool or graphics dependency in headless flight. Use Jolt math
   behind engine-owned contracts instead of adding a general math library.
   Build `game/flight/` fixed60Hz stepping, bounded validated next-tick commands,
   owning pose/velocity/force snapshots and declared hull/mass/thrust/contact
   envelope. Test acceleration/braking/mass and torque, invalid/capacity/stale
   failure, coasting, reset, replay and render-cadence independence through public
   interfaces. Preserve force/velocity traces. Astronomical frames/large-capital
   adoption remain M4 gates, not claims from double positions.
2. **M3-B: one controller, two intent sources.** Direct translation/attitude/brake
   and tactical destination/orbit commands feed the same limited forces/torques.
   Test mode changes under thrust/rotation without pose/momentum resets, arrival,
   orbit and out-of-envelope inputs. Do not build a universal planner or bypass
   physics with velocity/pose assignment. Input replay records tick-indexed
   validated commands, not wall-clock event timing.
3. **M3-C: tactical orbit and mounted first person.** Authored per-ship camera
   mount, explicit tactical/direct view switch, focus/zoom/free-look rules and
   presentation interpolation live outside simulation. Test two distinct mount
   definitions, fast turns, obstacle proximity, focus loss and cadence changes;
   camera work cannot mutate ship state. Document key/mouse bindings and pointer
   capture/release without hidden input on focus loss.
4. **M3-D: playable preset and renderer bridge.** Add `apps/flight_demo/` using the
   existing immutable accepted package and bounded game-owned scene definitions.
   Ship root motion/camera/lights/exhaust consume presentation snapshots through
   existing owners; any necessary generic transform API gets native pixel/lifetime
   tests. No renderer asset-name policy, new pass, temporal framework or material
   ABI change. Keep accepted charcoal hull/recessed bridge/droplet art; use a
   readable original flight HUD and shared100-asteroid geometry. Prewarm before
   measurement. Preserve before/after captures privately and provenance hashes.
5. **M3-E: bounded audio and delivery.** Enable existing SDL3 audio only in the
   flight graph; keep old/headless profiles isolated. Original procedural engine,
   thrust/brake and UI cues consume snapshots through a bounded mixer. Test mute,
   clipping, device loss/reconnect and repeated reset; audio failure leaves
   simulation running. Package one genuinely offline runnable demo with required
   runtime libraries/notices/local assets, no SDK/Conan/Internet startup step.
   Retain replay, benchmark/audio-video capture and owner control-feel feedback.

Use native CMake/Conan/CTest targets and existing Catch2. Each behavior follows
Red→Green→Refactor; build actual Clang debug/release/sanitizer and GCC consumers,
then current Windows MSVC/clang-cl compatibility where affected. Respect compiler
warnings, native ownership and bounded capacity; do not fabricate setup Reds,
strip flags, claim unrun tools or relax detectors to obtain Green. Record new
options/lock revisions and dependency licenses explicitly.

Final verification follows B1/B2 in [the benchmark specification](benchmarks-risks.md):
three warmed fixed input/camera runs; laptop1280×720/≤250ktriangles/1024shadow/
≤256MiBtextures, median≤50/P95≤66.7/P99≤100/max≤200ms, GPU-owned≤1GiB and process
WS≤6GiB. Record the separate RTX1920×1080 target/P95≤16.7ms, not a fabricated GTX
VM result without its display/access validation. Correctness/validation and clean
timing remain separate; report actual allocation/memory scope. Audio/video and
owner feedback are real completion gates, not substituted by screenshots.

Current checkpoint: the headless-double Jolt5.6.0 package and bounded128-body
adapter pass11 public physics cases on Clang debug/release, ASan+UBSan and GCC.
The Clang18 intrusive-reference diagnostic is resolved by simplifying shape
ownership without disabling checks; exact-head `a8918ef` hosted CI passes4/4.
Initial include-order/export-race/ASan PIE failures remain scoped as setup failures.

The shared controller now passes16 public cases for direct force/attitude,
coast/brake, feasible force-based speed limiting, tactical arrival/heading and
both orbit directions, co-moving mass, atomic orbit/mass rejection, copied and
bounded next-tick commands, owner-thread rejection, reset generations, mode
changes under thrust/spin and same-build cadence-independent replay. Four complete
headless graphs pass6/6 each, with final affected test reruns and twelve complete
Clang command analyses. Native GCC passes; GCC AST flags remain unsupported by
Clang tooling and are not stripped. No SDL/Vulkan appears in either headless binary.
See [controller evidence](../evidence/M3/controller/README.md) for exact scope.

The controller's exact-head `aae5f8a` hosted CI now passes4/4. Camera math now
passes13 public cases: per-ship mounts, shortest-path pose interpolation,
world-up tactical orbit, bounded drag/zoom and free look, copied target focus,
bounded obstacle/own-hull clearance, reset/gap snaps and stateless cadence.
Four complete graphs pass7/7, followed by final affected camera reruns in all
four configurations; six actual-command camera/header-consumer Clang checks pass.
Camera `c3805b6` hosted CI passes4/4. These are numerical contracts, not actual
SDL focus-loss or visual acceptance.

The generic renderer bridge now accepts a copied proper-rigid scene root:
geometry, normal transforms, point-light instances and transparent depth follow
it, while independent extras stay world-space. Two native GPU Reds are fixed;
root/copy/lighting/rotation/depth/extras/admission controls pass in four native
configurations. Six complete scene graphs pass69 CTests;35 actual-command Clang
header-consumer invocations pass after three affected probe lint reruns. This
adds no pass, resource owner, material ABI or authored asset change. At that
checkpoint the playable application was not implemented. Root `0fd447b` hosted CI
passes4/4.

The platform-neutral input value now passes8 public cases: held six-axis/brake
mapping, focus-loss neutralization/no stale resume, bounded persistent direct
pointer stick, consume-once tactical deltas, free-look steering isolation,
mode-change release, invalid/off-focus input and bounded camera event flood.
Four headless graphs pass8/8 each, with final affected reruns; six actual-command
Clang invocations pass after three equivalent test-assertion repairs. Actual SDL
window events and relative-pointer acceptance are not yet proven by these tests.

The contact bridge now copies up to127 static proxies into the controlled
hull's existing domain and preserves them across reset. A real wall-pass Red
(27.150021467m versus≤5.25m) is fixed. Eighteen controller cases and four8/8
headless graphs pass, with18 actual-command header-consumer Clang checks. These
are box proxies, not exact mesh contacts or a completed asteroid scene. The merged
pinned graphics/physics dependency graph is prepared for the playable application.

Input `c87d1e1` hosted CI passes4/4. The existing ImGui/SDL atlas and bounded
Context-overlay frontend is now shared by inspector widgets and custom flight
HUD drawing, not reimplemented with a second renderer backend. Inspector native
characterization stays green; a real custom-GUI admission Red is fixed and its
pixels, sequential context reuse and exclusive ownership pass. Six scene graphs
pass69 CTests, four native configurations pass with empty stderr, and nine actual
Clang implementation/header-consumer invocations pass. The original inspector
layout/style and GPU resource ownership remain unchanged.

GUI/contact `7b4615d` exact-head CI42957019450 passes4/4. The playable application
now renders the accepted immutable frigate and100 asteroid instances with matching
copied contacts. The authored default is a20m/s tactical orbit; direct view uses
the bow sensor mount. Double-origin presentation, bounded SDL event routing,
60Hz stepping, shared HUD/force-speed plots, fixed-tick replay, exclusive CSV/BMP
outputs and copied actuation traces are implemented. No new pass, GPU owner,
shader/material ABI or accepted appearance settings are introduced.

Five scene/presentation cases and headless CLI admission pass in four18/18 graphs;
shipping and shader-disabled graphs pass11/11 and10/10. Eighteen complete Clang
commands pass, followed by three affected implementation reruns after review.
Four330-frame native correctness runs pass with finite HDR, all GPU query rows,
empty stderr, strict sanitizer/leak checks and synthetic SDL keyboard/mouse/focus
routing. Actual ImGui reset clicks no longer mix old rendered and reset physical
snapshots. Native SDK relative-mode acceptance is proven; real OS focus loss,
physical control feel and current Windows behavior are not substituted by injected
events. See [playable bridge evidence](../evidence/M3/demo/README.md).

Playable `cf72519` exact-head hosted CI42970251740 passes4/4, including the new
Clang18 demo consumer checks. Original bounded48kHz audio now feeds an owning
SDL stream: eight procedural voices, a4096-frame input queue, native mute/clear,
reported device-loss/reopen and soft unavailable-driver recovery. Five mixer and
six native SDL cases pass in four20/20 graphs; four330-frame combined runs prove
reported loss does not stop physics, native M mute toggles, help and repeated
keyboard/GUI reset. The actual ALSA endpoint also opens quietly paused/muted;
listening/physical unplug are not inferred. Audio-disabled graphs and original
native probes stay green. See [audio evidence](../evidence/M3/audio/README.md).

Audio `2837627` exact-head hosted CI42982673426 passes4/4. Offline packaging
and the [current Windows handoff](m3-windows-handoff.md) are implemented. Four
package-enabled graphs pass21/21, eight filesystem/process-boundary cases pass,
and native frame clocks correlate in all four configurations. The preliminary
Linux bundle starts/renders with networking unshared and Conan hidden; final
packaging must follow its source commit. See [packaging evidence](../evidence/M3/package/README.md).

Current behavior source `95645f6` passes exact-head CI43077400206,4/4. Private v7
bundling verifies its complete manifest/source archive and renders with networking
unshared and Conan hidden. Owner feedback repaired off-screen HUD projection,
steering response and stale-autopilot TAB handoff without changing accepted art.
The final0.37rad/s steering, bounded braking/coasting, handoff, mute/listening and
real playback-device recovery are confirmed; corrected original audio/video stays
private. See [final laptop/owner evidence](../evidence/M3/laptop/README.md).

Three final-source warmed2400-frame laptop runs pass B1 with every measured frame
retained: medians16.67–16.69ms, P95≤18.48ms and maximum20.99ms. Allocation, resident
and VMA observations retain their actual scope; no global/continuous GPU-memory or
scanout proof is inferred. Correctness and instrumented recording are separate.

Next: finish current Windows MSVC/ClangCl, app-local offline startup and scoped RTX
timing/memory, plus the remaining non-UI M3 implementation/delivery checks.
Owner-confirmed Windows disconnected-network startup now succeeds.

The owner approved matching lower opposing RCS banks for turning, rolling and
strafing before the UI pass. The [RCS candidate](../evidence/M3/rcs/README.md) now
has 16 banks and 32 socket plumes after the owner's rear-visibility request,
driven by actual interval force/torque and stabilization rather than keys.
The [aft correction](../evidence/M3/rcs/aft/README.md) moves eight obstructed jets
onto outer fairings and adds eight aft jets, retaining the accepted forward look.
Original M2 prefixes, rear exhaust and physical traces remain exact;
249,942 workload triangles and 247 ship instances stay inside
existing limits. Four native graphs, affected Clang analyses, native rig/replay
checks and a clean 2,400-frame 1080p allocation replay pass. See the
[scoped implementation plan](m3-rcs.md). The owner accepts every location and
requests demand-driven puffs. The [pulse implementation](../evidence/M3/rcs/pulses/README.md)
now gates complete calibrated patterns once per successful 60 Hz tick, with quiet
intervals, residual hysteresis and continuous exhaust only at saturation. The
then-approved asset and physical traces remained unchanged at that checkpoint.

The owner then authorized nozzle re-aiming to correct simultaneous counteracting
jets. The [directional rig](../evidence/M3/rcs/directional/README.md) preserves all
32 socket pivots and mount plates, re-aims sleeves/throats/plumes together, and
uses sparse per-axis minimum-impulse couples instead of forced bank participation.
Three paired banks turn pitch/yaw and two turn roll; arrest uses disjoint sets.
Only 2,204 original forward socket vertices change; all other original vertices,
UVs/handedness, other section prefixes, main exhaust and physical traces are exact.
All 32 rendered centerlines are clear; the workload stays at 249,942 triangles.
Owner re-aim/physical-wrench preview review and affected final Windows/bundle/budget
checks remain open; this is not M3 acceptance.

The owner subsequently chose [physical RCS](../evidence/M3/rcs/physical/README.md),
not a post-physics visual approximation. The graphics-free simulation now owns
bank definitions and valve state, applies actual force and moment from firings to
Jolt, and publishes the same firing states used by rendering. The old application
pulse clock is removed. Main-drive isolation, configured authority, real quiet
coast, opposing arrest and speed admission pass public tests. Eight native graphs
and 60 actual Clang command analyses pass; the current full-HD replay has zero warm
allocations. The earlier exact-motion-match claims are historical because physical
pulsing intentionally changes trajectories. Owner gameplay and fresh Windows/hardware
acceptance are pending; see [ADR 0017](../adr/0017-physical-rcs-actuation.md).

The post-environment-fix ClangCl baseline reaches both configurations and the full
RTX replay. Its first CIM query fails with no GPU-memory samples, and its clean
replay reports 2 warm ImGui allocations. Preserve these findings until verified
repairs or successful current-source checks; Linux zero-allocation results do not
waive the Windows findings. Parent `6a41458` hosted CI passes all four jobs.

The owner requests another UI refinement pass **after all other M3 work, but before
M3 completion**. Keep it queued until those remaining technical checks are finished;
then use all three EVE Online articles as references:

- https://www.eveonline.com/news/view/photon-ui-iterating-and-improving
- https://www.eveonline.com/news/view/eve-evolved-a-new-horizon-for-the-hud
- https://www.eveonline.com/news/view/evolution-not-revolution

Review all three when the UI phase begins and settle the concrete panel/control
scope then; these references are not an implemented or approved UI design. Use
layout, hierarchy and interaction principles, not copied EVE assets or branding.
Preserve accepted ship/material/effect policy and physical control contracts.
Verify the UI changes and rerun affected acceptance/performance checks before
final review and normal reviewed integration. Do not mark M3 complete or merge it
before this requested pass is implemented and reviewed with the owner.

The owner confirms the [v13 shared-desktop audio repair](../evidence/M3/audio-desktop/README.md)
works; a reported recurrence was an accidental v1 launch. Linux audio no longer
uses direct ALSA hardware. The owner also explicitly accepts v13's physical RCS
handling and nozzle aim. The [current Windows return](../evidence/M3/windows/current-physical/README.md)
passes both frontends/configurations, all physical walkthroughs and zero warm
allocations in clean/memory replays. ClangCl has complete sampled WDDM coverage
and app-local startup. The later MSVC Release rerun again has no GPU-memory
samples and reports 2 warm ImGui allocations. The [tooltip repair](../evidence/M3/tooltip/README.md)
passes locally; native Windows confirmation remains open. Do not request another
unchanged full rerun or waive either finding. The later
[explicit PDH CSV comparison](../evidence/M3/windows/provider-comparison/pdh-csv-return.md)
returns matching process-memory gauges before/after three CIM timeouts and a
successful raw read. The precise Windows cause remains unknown. A bounded native
PDH workload collector is implemented with unchanged coverage and budgets. The
[ClangCl](../evidence/M3/windows/pdh-collector/clangcl.md) and
[MSVC](../evidence/M3/windows/pdh-collector/msvc.md) native returns at `7afeefe`
pass both configurations, sampled memory coverage and native F12 system/device
readings (88 CTests combined). Both miss the unchanged 16.7 ms RTX P95 target:
21.8084 / 21.8387 ms. CSV completion cadence is now near 60 Hz versus roughly
144 Hz historically. The owner reports using Artemis Virtual Desktop, a plausible
but unproved presentation confound. Retain functional/memory results and defer
final RTX timing to a direct desktop session after the UI pass; do not patch
rendering, relax limits or request an unchanged full rebuild on these timings.
Explicit live-panel hovered-tooltip and final acceptance remain open.

The owner-requested [F12 performance overlay](m3-performance-overlay.md) is
implemented before the broader UI pass. Native Linux verifies hidden startup,
visibility persistence, detailed rendering/allocator metrics, whole-system CPU/RAM,
shared/UMA labelling and allocation-free live-panel replays. The selected-adapter
Windows PDH source now also has scoped native MSVC/ClangCl confirmation. It is
not a substitute for the per-process GPU-memory budget observer or final owner
UI acceptance.

The owner subsequently prioritized the [immersive UI pass](m3-ui.md) while
remote timing is deferred, superseding the earlier technical-checks-first queue.
The first runtime pass is implemented and locally verified: no persistent flight
panels, on-demand controls/data/performance, contextual target actions and shared
supplied branding. [UI verification](../evidence/M3/ui/checks.md) records four
native configurations, real hover/tooltip tests and unchanged physical replay.
This is a starting point for owner refinement, not final UI or M3 acceptance.

M3 remains open until these gates and exact post-merge CI complete. No mandatory
tracker workflow or per-command reapproval is introduced.

After M3 is completed and merged, the owner requests another refinement of the
milestones and executive brief. Convert that agreed plan into GitHub milestones
and a Project kanban board so work is easier to navigate and resume. This follows
M3 closure; do not rewrite the roadmap or create the tracker structure during M3.
