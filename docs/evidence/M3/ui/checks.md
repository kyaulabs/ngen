# Immersive HUD first-pass verification

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
