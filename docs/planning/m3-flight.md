# M3 offline flight implementation

M2 delivery completed before this work: [PR#9](https://github.com/kyaulabs/ngen/pull/9)
merged the verified tree at `4b554cc`; post-merge
[CI42920708365](https://github.com/kyaulabs/ngen/actions/runs/42920708365) passes4/4.
The completed branch and clean review worktrees are removed. Current M3 branch
starts from that exact integration. M3 is **not implemented or accepted** by this
plan; execute the five [milestone outcomes](milestones.md#M3--ship-the-polished-offline-flight-demonstration).

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

Current implementation checkpoint: the separately identified headless-double
Jolt5.6.0 package and bounded128-body local adapter build on Clang debug/release,
ASan+UBSan and GCC. Ten public physics cases cover force/mass, owned precision,
coast/brake, contacts, torque, admission/foreign/thread/capacity boundaries and
rejected impulses/mass changes that would trigger native velocity clipping.
Force/mass/impulse regressions have actual behavioral Reds/Greens. No SDL/Vulkan
appears in the headless binary dependencies. The initial package include-order
failure, concurrent Conan-export race and ASan PIE/shadow startup failure are
setup failures, not behavioral Reds; they remain recorded. Serialized preparation
and the existing host `-no-pie` ASan link configuration pass with sanitizers intact.

Next: finish same-session verification/review of this adapter slice, then build
the shared direct/tactical flight controller, bounded command/snapshot replay and
camera contracts. These backend tests do not complete M3-A or the demo. All five slices, native
verification, same-session review, signed commits and normal reviewed integration
must finish before M3 is declared complete. No mandatory tracker workflow or
per-command reapproval is introduced.
