# Motion and final-art same-session review

This is an assistant-operated review under `kyau`, not a different reviewer or
independent human approval. Final PR review and scoped Windows execution remain
open.

Reviewed the eight changed C++/shader source files against `d036c91`, their native
probe callers, reload publication order, material packing/admission, generated
layout consumers and the six private-asset authoring steps. Verified that package
replacement and fixture/binding updates occur before construction of the frame's
borrowed flow table; a suspected stale-table-on-reload issue is not present.

The renderer validates the complete table before submission, bounds quantization,
keeps the 64-byte GPU material ABI and copies constants through the existing
Context owner. Source-node seeds are presentation variation, not identity/security
keys. Disabled flow restores default rendering. The workload owns the asset-name
binding; no camera/light padding or renderer name policy is introduced.

Concrete review corrections were verified:

- Bound normalized UV guidance before lattice integer conversion. The original
  large-finite-UV finiteness probe passed on this GPU; bounded-guide equivalence
  failed, then passed. This is not evidence of an observed NaN/crash.
- Decode half floats for the continuity metric instead of measuring half-bit
  distance across changing ULP spacing. A deliberate phase jump still fails the
  new detector; advection/core/tip/seed/disabled/copy controls pass.
- Remove signed integer promotion in half decoding and use `std::lround` for
  nonnegative, validated unit-domain amount quantization. The two original
  clang-tidy diagnostics and affected reruns are retained.

Actual checks: seven native build/CTest graphs, eight final affected native
resource/inspector runs, formatter/whitespace, 111 successful actual-command Clang
analyses, final-package 600/60 steady and four-reload/two-resize measurements with
all 1,200 rows verified, native final geometry rendering/ASan admission, and exact
six-stage current-host asset replay. GCC native execution passes; its actual
module flags remain incompatible with clang-tidy. Unchanged shader-offline work
was reused only after the later C++-only lint fixes.

The final art changes are scoped separately from renderer architecture. Materials,
images and accepted hull/exhaust data remain fixed during the authorized window
cut. Unrelated primitive attributes/topology and original instance bytes are
verified; all three aft captures remain byte-identical. Six-window sightlines
prove 0.9 m recesses at all three LODs rather than transparency over solid hull.
The workload remains 247,542 triangles and 211 draw records. Original/private
assets and reference pixels are not committed.

No further actionable source defect was demonstrated in this review. This does
not prove arbitrary device-loss handling, physical CFD/refraction, scanout,
continuous process-memory peaks, cross-platform asset byte replay or new Windows
execution. Earlier static Windows/compiler/memory evidence keeps its exact scope.
