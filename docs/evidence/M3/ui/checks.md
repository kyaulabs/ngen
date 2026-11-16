# Immersive HUD first-pass verification

This first-pass 1.0-brand record is historical. The owner's replacement Graphite
1.2 pack, flight-local 65% surfaces/headers/borders and private Linux recorder are
covered by [Graphite verification](graphite.md). The current owner-requested
[Visual Identity 1.4](brand14.md) supersedes that baseline; earlier evidence is
not relabelled.

This implements the owner's requested starting point, not final UI or M3
acceptance. Private rendered captures and logs remain under `build/m3-flight/`.
The rejected boxed deck was not implemented. See the
[UI direction](../../../planning/m3-ui.md) and
[visual study](../../../planning/m3-ui-visual-research.md).

- Flight starts without panels: supplied small mark, floating speed/mode,
  published main-drive level and actual firing-bank count, selected-target
  identity/range and a controls hint. Muted/disconnected audio is labelled.
- F1 opens controls; F2 opens flight data/plots; F12 remains the separate
  system/device overlay. Escape closes controls and releases input. Existing
  flight shortcuts remain. Target buttons reveal on hover, not while steering
  or through an opaque utility/performance panel.
- The inspector keeps its control arrangement and 13 px technical font. Its
  header is taller to fit the supplied proportional wordmark; shared colours
  use the supplied dark tokens. Flight body/readout text uses an available
  installed system sans font, with the existing embedded MIT font as fallback.
  No system fonts are redistributed or installed.
- Standard-library build tooling decodes the supplied wordmark, mark and app
  tile into one generated header. Explicit PNG input/chunk/decompression/
  dimension and token limits bound the generated artwork. Output paths are
  owned CMake build artifacts, not an arbitrary-file import interface. The
  original `brand/` files are unchanged.
  All three decoded RGBA images match FFmpeg byte-for-byte. The owned SDL
  window requests the supplied icon; Linux reports success. Unsupported icon
  application is nonfatal and reported, not claimed as success.
- Artwork shares the existing GUI atlas, Context owner and overlay pass. The
  overlay shader now samples RGB as well as alpha and decodes sRGB artwork;
  white glyph pixels preserve text colouring. No renderer/font dependency,
  new GPU owner/pass, capacity increase or material ABI change was introduced.
  Corresponding-source archives now include `brand/` and its generator inputs.

Verification used the declared demo graphs, not edited compile commands:

- Clang Debug/Release, strict ASan+UBSan/leaks and GCC: 26 CTests each, 104 total;
  affected CLI/layout/brand/package tests were rerun after final UI repairs.
  Five brand tests include all PNG row filters, RGB/RGBA preservation, malformed
  and expanding inputs, and generated supplied-artwork metadata. Tests do not
  create source-tree bytecode that would dirty corresponding-source packaging.
- Six affected C++ translation units/header consumers in each Clang graph:
  `engine/tools/{gui,ui}.cpp`, `apps/flight_demo/{app,hud_layout}.cpp`,
  `tests/flight_hud.cpp`, `apps/inspector_probe/main.cpp`. All 18 actual-command
  analyses pass; `app.cpp` was rechecked in all three after final hover repairs.
  Native GCC builds pass; no GCC AST compatibility claim is added. Formatting
  and four PowerShell boundary suites pass.
- Four native inspector probes pass, including a coloured-atlas pixel
  regression for supplied cyan `(99, 221, 248)`. Four final 330-frame validated
  SDL walkthroughs pass physical-input/reset, audio-state and F1/F2/F12 checks,
  with empty stderr. Strict sanitizers cover these exercised paths only.
- Six opt-in rendered/owned-SDL tests pass in Debug: supplied cyan/violet
  artwork, absence of the old telemetry slab, F2 after warmup, real target hover,
  suppression of invisible target buttons under F12, and 320 × 180 admission.
  Three non-debugger rendered contracts also pass under strict sanitizers.
  Pure layout tests cover 320 × 180 through 1920 × 1080, including separated
  controls/performance bounds. Tiny F12 panels request a larger window for full
  counters instead of presenting clipped telemetry as complete.
- Two 2,400-frame Debug replays open F2 after warmup and follow the actual public
  plot position using events addressed to the discovered owned SDL window.
  Both prove an actual `SetTooltipV` call and zero warm C++/ImGui allocation
  calls, including the live F12 combination. The untraced Release replay also
  has zero warm calls, 2,400 GPU rows, and all 27 physical fields identical to
  the previous full replay. These are correctness/allocation checks, not final
  timing or complete process/device-memory acceptance.

Same-session review repaired three concrete UI defects through observed Reds:
world-target hover inherited the fallback ImGui window's clip rectangle; F12
could cover still-active contextual buttons; and the first post-warmup plot
hover allocated a 64-byte draw-data layer. The tooltip fix renders two cold,
unsubmitted preparation frames because auto-sized windows skip their first
frame. Geometry reservations and the existing warmup threshold remain intact.
The initial hover fixture inspected a variadic argument before its debugger
location was valid; only the corrected public `TextV` fixture's Red/Green runs
are behavioral evidence. No detector or allocation counter was disabled.

Actual default, expanded-data and shared-inspector captures were opened for
visual review. No further task-related blocking defect was found in this
same-session review. Owner refinement, independent final review, native Windows
post-UI/hovered-panel checks, direct-desktop RTX timing, final three-run budgets
and final delivery/recording acceptance remain open. Artemis timing misses are
retained; no threshold, rendering/presentation setting or audio gain was changed.

The private Linux v15 UI preview is built from signed source `754c72d` at
`build/m3-offline-linux-v15/ngen-flight/run.sh`. Its 25 manifest hashes and all
414 corresponding-source archive members are verified, including original brand
inputs. App-local geometry verification and a 310-frame F1/F2/F12 walkthrough
pass with networking unshared, Conan hidden and SDK environment removed. The
muted desktop Pulse client leaves card profiles, sinks and default routing
unchanged. See the [bounded metadata](offline-preview.json); this establishes
scoped offline startup, not final budgets, listening or owner acceptance.

The subsequent owner refinement removes the F1/F2 wordmarks, retaining only the
upper-left flight mark and the existing inspector branding. F1/F2/F12 now fit
content vertically at fixed readable widths, with viewport height caps. Actual
window rectangles govern target-action occlusion. A native public-geometry Red
showed 155 px of unused F1 space; the new test checks all three panels and
shrinking back from F2 to F1. Eight scoped CTests, six actual-command Clang
analyses, eight native UI contracts, strict sanitizer rendered checks and the
validated GUI reset pass. Both 2,400-frame actual plot-hover paths remain free
of warm C++/ImGui allocations. Final timing and Windows gates remain deferred.

The next owner refinement removes runtime shortcut annotations, the floating
binding reminder and keyboard/mouse tutorial prose, including binding references
in status messages and the inspector reload label. Action names, telemetry and
all existing bindings remain. No settings page is added. A native public font
rendering capture first observed the old hints, then verifies their absence and
retained action labels across the walkthrough. Twelve selected CTests in four
graphs, six Clang analyses, nine native UI cases, sanitizer rendering, inspector
probes and the retained SDL input/reset walkthrough pass.

One initial live-performance plot-hover replay reported two warm ImGui calls.
A 180-frame trace, two complete 2,400-frame allocation traces and a subsequent
normal GDB-controlled 2,400-frame fixture report zero; the latter still proves
the actual hover. The first result is retained under `build/m3-flight/`; its cause is not
established and no allocation repair is claimed by this text-only refinement.
Intermittent allocation confirmation remains part of final M3 verification.

The next refinement moves Approach, Orbit and Focus exclusively to the selected
target's right-click popup. Selection/hover and ordinary controls/data panels no
longer expose those buttons. A brief click opens the popup; movement beyond the
existing ImGui drag threshold or a hold beyond 250 ms retains RMB camera/steering
without opening it. Actions use the captured target, and dismissal, focus loss,
mode/reset and panel handoffs clear stale requests. Popup edge placement and
blocking over the read-only performance overlay are exercised through owned SDL
input and public GUI geometry. No shortcut annotations are added.

Fourteen native UI cases pass, including actual menu-item Focus dispatch,
Escape/selection closure, retained camera dragging, bounds and zero warm
allocation calls in the exercised context-menu paths. Four sanitizer-rendered
cases exercise the popup without a debugger. Twenty-four selected CTests across
four graphs, three configuration-specific Clang checks, validated control/audio
walkthroughs and unchanged 27-field physical traces pass. Private popup imagery
was opened for visual review; no pass, GPU owner, resource capacity or dependency
was added. This is same-session review, not final independent/M3 acceptance.

The recurring two-call hover result was reproduced with allocation tracing
**disabled during cold startup**, then enabled before the measured window.
Both 224-byte allocations occur in ImGui's input-event queue/trail on the first
post-warmup mouse input, not in tooltip geometry. Tracing cold allocations had
changed startup timing and obscured this reproduction. The existing unsubmitted
preparation frames now exercise the public mouse-position event API and restore
the initial GUI position; no SDL event, flight intent or OS pointer is injected.
This warms the library's default storage without changing capacities or the
60-frame measurement warmup. The short traced reproduction and both full
2,400-frame actual-hover fixtures now report zero warm C++/ImGui calls. Earlier
untraced reports are retained rather than individually attributed from this stack.

Follow-up review caught a short Alt+RMB click also opening the popup. A native
modifier-event Red reproduces that conflict; checking GUI/native modifier state
at gesture admission and release preserves camera-only free look. All 15 native
UI cases and affected input/camera/CLI consumers pass after the fix, with the
three configuration-specific Clang checks repeated. No binding was remapped.

The owner then reported cursor restoration to the first context-click location
when reopening at another target. A native two-position regression confirms that
ordinary menu clicks were entering SDL relative mode twice even though both
popups used the correct locations. SDL's relative-mode exit restores the pointer;
context clicks now avoid that cycle entirely. Logical input still responds
immediately, while OS relative capture starts on drag/hold or camera-only input.
The CSV request flag now records that actual relative-mode intent, independently
of the logical button state; failed requested capture is not hidden.

The reopened-menu regression checks the new popup position, absolute cursor mode
and absence of explicit warp requests. Seventeen native UI cases, four strict
sanitizer-rendered cases, 24 selected CTests, three Clang checks and validated
control/audio walkthroughs pass; all 27 physical fields remain unchanged.
Flight controls/data, performance and context-menu background draws use alpha
166/255 (about 65%), while heading text remains 255/255. The alpha regression
observes actual public draw/font calls, not source text or a global style value.
A fresh private context-menu capture was visually reviewed and the 2,400-frame
live-performance plot-hover check retains zero warm C++/ImGui allocation calls.

The owner requested another 10–20 percentage points of transparency. Current
flight menu/panel backgrounds use 50% opacity (128/255), 15 points below the prior
65%; headings remain fully opaque. The focused native draw-alpha regression
passes for controls/data, performance and the context popup, and all four native
flight consumers rebuild successfully. No input or renderer contract changed.
