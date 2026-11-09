# Complete ship bounds

The owner reported that the inspector box missed the rear engines/exhaust. The
box used the selected primitive's local bounds and instance transform: selecting
an armor component boxed that component, not the assembled spacecraft.

`InspectorUi` now projects the package's complete world bounds, which already
include every authored geometry instance and its transform. Selecting or clearing
a material/part no longer shrinks the ship box or applies its transform twice.
The checkbox is named **Ship bounds**. Rendering, emission and bloom strength are
unchanged; these are geometry bounds, not a new post-effect or collision bound.

A checked two-instance fixture places the hull at world x=[3,5] and a separately
instanced rear component at x=[7,9]. Through the public UI frame/overlay interface,
all part selections and no selection must produce the complete projected x=[400,880]
wire box. Scaled antialiasing is allowed, not the old 320-pixel truncation. The
original implementation fails with the corrected scaled-stroke tolerance; the
full-asset implementation passes. The initial tolerance underestimated a 2x line
stroke (observed x=[398,884]); that fixture expectation was corrected and Red was
reverified against the original selected-component behavior, not weakened to hide
it. Explicit optional admission is retained after the compilation-aware checker
could not infer the custom assertion helper's control flow.

Clang Debug/Release/ASan+UBSan/leaks and GCC Debug pass the actual native probe
and **12/12 CTests per graph**. Six actual Clang TU/configuration analyses cover
the changed UI and native probe. GCC compilation/execution passes; clang-tidy still
rejects its real module flags, which are not stripped. The four physical inspector
process cases also pass in Release and ASan, including warmed allocation controls
and repeated-reload/resize behavior. No production Vulkan/shader ABI changes or
new dependencies are introduced.

The canonical private ship was rendered with ordinary validation and a stabilized
three-frame inspector capture. Actual pixels were inspected: the wire box now
reaches the rear engine geometry, the tabs are present, and shared 16-pixel side
gutters remain. Captures, model and package bytes remain private; only identities
and numeric/text results are retained. This is local execution and agent inspection,
not a fresh Windows result for this UI change. The owner subsequently accepted
the specific corrected rear-engine box; that does not accept unrelated M2 gates.

The owner accepted offline Slang for M2 but explicitly kept resize investigation
blocking. The bounding-box correction does not waive or claim to repair the
separate resize latency failure. M3 implementation has not started.
