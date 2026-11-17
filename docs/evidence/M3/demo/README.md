# Playable flight bridge

`flight_demo` now renders the accepted immutable frigate and100 instanced
asteroids through the existing Context/SceneRenderer/Gui owners. Tactical
orbit is the authored default:20m/s around the first asteroid at600m. Direct
mode uses the ship's authored bow sensor, not a universal camera offset.
Mode changes preserve physical motion; release coasts and SPACE applies
bounded opposing force. No package recook, accepted material/effect adjustment,
new render pass/backend, resource owner or material ABI change is introduced.

The geometric preset verifies package compatibility, proper-rigid asset roots,
opaque-hull enclosure and the250000-triangle/256-draw envelope. The accepted
package produces247542 triangles and100 copied static proxies. Fifty asteroids
retain near-field context; fifty extend ahead of the bow sensor. Native contacts
and camera spheres derive from the same rendered transforms. These are
conservative boxes/spheres, not exact mesh contacts or arbitrary visibility proof.
CRC32 is a compatibility/corruption marker, not authentication.

Presentation subtracts the double ship origin before float narrowing, composes
interpolated ship pose with the asset root, constructs camera/shadow matrices
and shifts static extras without modifying their owning preset. One bounded SDL
loop feeds the existing input/controller/camera contracts at60Hz. The shared
bitmap-atlas HUD plots speed/force and exposes mode, navigation, focus and reset.
CSV retains owning state, quaternion/angular velocity, camera/render pose,
actuation, generation, memory, pass queries and input/capture state. Outputs are
exclusive; invalid options/packages reject before native startup.

Runtime Reds include missing preset/presentation/startup/frame loop, an empty
forward field, accepted non-rigid roots, absent SDL TAB/W routing, a coast-only
default, irreversible focus, aft-drive leakage during lateral steering and a
GUI reset mixing0.880427279742m-old rendered pose with newly reset state.
The drive now uses the before-tick attitude for its interval wrench. All reset
sources defer replacement until the next unfrozen frame boundary. The initial
braking check incorrectly expected an immediate stop; the unchanged controller
correctly uses bounded proportional opposing force with a0.35s response. That
assertion error is retained separately, not a production physics defect.

Four complete native Clang debug/release, strict ASan+UBSan+leaks and GCC graphs
pass18/18 CTests each. Shipping and shader-disabled consumers pass11/11 and10/10.
Five public scene cases cover fixture/contact admission, forward depth, identity/
rotated presentation, invalid camera values and improper asset roots. Three
headless CLI tests cover admission, owner-file preservation and invalid packages.
Eighteen actual Clang commands cover every new-header consumer and the new
allocation-probe target configuration; all pass, followed by three current
implementation reruns. Initial explicit-float, const-reference and trivial-move
lint findings were repaired without suppressions. GCC native passes; its unchanged
module flags remain unsupported by Clang AST tooling and are not stripped.

Four current-source330-frame native correctness runs pass with finite HDR,
330/330 GPU query rows and empty stderr. Injected SDL events exercise held
thrust, coasting, bounded braking, lateral steering, relative-pointer API
acceptance, focus neutralization/no stale resume, mode/focus toggles, keyboard
reset and an actual SDL-to-ImGui reset-button click. Every fixed-tick rendered
pose matches its physical generation. `walkthrough.csv` is the Release trace.
Injected focus events are not proof of real OS demotion or owner control feel.
Native SDK acceptance is not scanout/arbitrary device-loss proof.

These validation runs are not optimized timings. Their C++ counters include
validation-layer C++ activity; sanitizer builds explicitly report
`allocation_probe=0`, not an observed zero. ImGui has separate accounting.
Neither counter covers C/driver heaps. VMA statistics cover Context allocations,
not whole-process GPU memory or continuous global peaks. Earlier private
1280×720 tactical/first-person BMPs remain outside Git; their hashes and earlier
source scope are retained, not relabeled as final captures.

Build with `sh tools/prepare.sh demo-release`, `cmake --preset demo-release` and
`cmake --build --preset demo-release`. Run `flight_demo scene.ngas --verify` for
headless authored-profile admission; use `--replay --frames=N --warmup=60` for
the versioned fixed-tick command/camera script, or `--walkthrough --frames=330`
for the synthetic SDL/GUI correctness sequence. Leave physical input idle during
scripted tests. `--report=new.csv` and `--capture=new.bmp` never overwrite files.
RMB steers/orbits, ALT+RMB free-looks, wheel zooms, TAB changes mode, G/O/F control
selected-target approach/orbit/focus, R resets, ESC releases and F1 shows help.

Same-session Core/C++ review fixed the demonstrated root, focus, actuation and
reset defects; see `review.md`. This is not independent review. Parent GUI/contact
head `7b4615d` hosted CI42957019450 passes4/4; those checks do not establish the
new demo's hosted status. Audio is explicitly pending. Offline packaging,
current Windows, three warmed laptop replays, scoped RTX timing, recording and
owner feedback remain. **M3 is not complete.** Accepted M2 art is not reopened.
