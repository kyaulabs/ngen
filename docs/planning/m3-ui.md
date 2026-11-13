# M3 immersive flight HUD and shared brand integration

The owner rejected the first visual mockup as too panel-heavy and requested a
more immersive view. This supersedes the initial boxed deck/right target-panel
proposal: no persistent panels, a minimal bottom speed/mode/actual-actuation
readout, world-anchored selected-target information, and contextual actions.
Flight data, controls/help and F12 appear only on demand. Shared NGEN identity
remains approved. The inspector keeps its layout and behaviour; shared colours
and header artwork adopt the brand.
Local RTX acceptance remains deferred until the owner can use a direct desktop
session after this pass. Artemis timings remain recorded, not waived.

The three requested references were read in full. The initial proposal had not
opened their images; after the owner called out that gap, all 27 relevant UI
images were visually inspected. See the [visual study](m3-ui-visual-research.md)
for image coverage, observations and concrete design consequences.

- [Photon iteration](https://www.eveonline.com/news/view/photon-ui-iterating-and-improving):
  compact grouping, quiet inactive surfaces and restrained highlights; no grain.
- [HUD principles](https://www.eveonline.com/news/view/eve-evolved-a-new-horizon-for-the-hud):
  legibility, discoverable actions and truthful state without changing gameplay.
- [Evolution, not revolution](https://www.eveonline.com/news/view/evolution-not-revolution):
  group related information/actions, separate speed from unrelated systems,
  expose shortcuts and protect screen space. Do not copy EVE artwork/branding or
  add its health, capacitor, modules, minimap or other unimplemented gameplay.

Use the supplied NGEN Visual Identity 1.0 guide and dark semantic tokens in
`brand/web/ngen-tokens.json`: quiet blue-black fields, cyan primary actions/focus,
violet secondary accents, labelled warning/error states. Preserve original brand
files. Render the supplied wordmark proportionally with its clear space and at
least 144 px visible width in help/details. During flight use only the small
supplied standalone mark; the owned window also uses the supplied icon. No
redrawn logo, animated noise, splash/menu system, new font/renderer dependency or
global desktop installation.

The default HUD has no opaque deck, target card, module grid, top bar or boxed
binding banner. Speed/mode and actual main-drive/RCS values float near the bottom;
target identity/range follow the selected object's screen position. Existing
approach/orbit/focus actions are contextual, not another persistent panel.
View/reset/audio controls and optional flight plots remain accessible on demand;
F12 stays the separate system/device performance panel. F1 keeps controls
discoverable. The [immersive mockup](../mockups/m3-flight-immersive.svg) supersedes
the earlier panel-heavy [mockup](../mockups/m3-flight-deck.svg). Preserve every
accepted shortcut, capture/focus/reset rule, motion and audio contract. No speed
setter, synthetic health gauge or new navigation authority.

Implementation slices, each through public-boundary Red → Green → Refactor:

1. Compile the supplied brand images/tokens at build time with bounded standard-
   library tooling; render them through the existing GUI atlas and set the owned
   window icon. Preserve Context ownership, one atlas/overlay pass and all limits.
2. Apply shared semantic colours and approved header artwork without rearranging
   the inspector. Package the corresponding brand sources for offline delivery.
3. Replace the flight layout with the immersive readout/contextual-action design.
   Keep 1280 × 720 and 1920 × 1080 readable, and provide a bounded compact
   arrangement down to the
   existing 320 × 180 admission rather than overflowing or increasing capacities.
4. Exercise real owned-window SDL actions, data expansion, help/F12 combinations,
   resize/compact layout and hovered plots; keep zero warm allocations in the
   exercised steady paths. Update the old coordinate-based fixtures to prove an
   actual plot hover, not merely move the pointer where the plot used to be.

Verify affected targets/header consumers in Clang Debug/Release/strict ASan/UBSan
and GCC, actual-command AST checks, package tests, shared inspector behaviour and
native GUI captures. Same-session review does not replace final independent/owner
review. Supply an offline Linux UI preview; do not claim fresh native Windows,
local RTX timing, final three-run budgets or M3 completion from local checks.

The owner authorized this direction as a starting point for later refinement.
The first runtime pass is now implemented: F1 controls, F2 data/plots, F12
performance, hover-only target actions and shared branding through the existing
atlas. The inspector keeps its control arrangement and technical font sizing;
its header grows to fit the supplied wordmark. See the
[verification record](../evidence/M3/ui/checks.md) for actual tests and review
repairs. Private default/data/inspector captures have been visually reviewed.
The private offline Linux v15 preview is available from source `754c72d`; its
manifest/source correspondence and isolated startup are verified. Owner verdict,
final native Windows/direct-desktop timing and M3 acceptance remain open.
