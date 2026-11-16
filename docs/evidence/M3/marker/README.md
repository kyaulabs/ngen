# Selected-target HUD projection repair

The first hands-on window exited with `overlay positions must be finite local
pixels`. Its selected-target marker tested homogeneous depth, but not horizontal
or vertical clip bounds. A target near the camera plane could therefore produce
millions of GUI-local pixels and correctly fail Context's overlay admission.

The existing projection is extracted into application-owned `project_target`.
A visible-target characterization passes before the repair. Two runtime Reds
then prove off-screen camera-plane targets and invalid viewport/homogeneous inputs
could publish invalid pixels. The repaired helper checks finite bounded GUI size,
finite clip coordinates and the complete Vulkan clip volume before division.
Off-screen markers are omitted, not clamped into misleading positions. The HUD
uses this one implementation; renderer admission, camera/physics behavior, assets,
passes, ownership and material ABI remain unchanged.

Three new scene cases bring that executable to8 cases. Four full21-test graphs
pass, with final affected test reruns. Twelve complete native Clang commands cover
`view.hpp` consumers: view, application, main and scene tests in three profiles.
Nine pass initially; the three test commands pass after an explicit optional guard
(the analyzer does not infer the Catch2 fatal assertion). No checks are disabled.
GCC builds/tests pass; its previously disclosed AST limitation remains.

Four330-frame native SDL/GUI/audio correctness runs finish with finite HDR,
all GPU timing rows and empty stderr under Vulkan validation and strict sanitizer
settings. The first ASan/GCC checker results missed the actual GUI-button reset;
unchanged affected native reruns pass. The original results remain, and their
cause is not proven. Synthetic controls/device events still do not establish
physical input, listening, reconnect or scanout acceptance.

Same-session review traced the helper, HUD caller, GUI conversion and unchanged
16384-pixel renderer guard. The near-plane reproduction is the concrete defect;
physical continuation must still confirm the owner's original session no longer
exits. No unrelated refactoring or new dependency is included.

The owner confirmed only that movement looked fine and heard sound was good but
quiet; they had not tried controls and mistook default tactical autopilot for a
script. That is not control/reconnect acceptance. Explain TAB/direct control and
reopen the repaired interactive bundle before requesting that verdict.
