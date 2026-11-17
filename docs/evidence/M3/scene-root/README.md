# Copied rigid scene presentation

`SceneView::root_transform` adds a copied proper-rigid transform for an immutable
scene. Rotation is unit/orthogonal within1e-4 with positive determinant; local
translation is bounded to±16384m. Nonfinite, projective, mirrored, scaled,
sheared and out-of-range roots reject without publishing a frame. Matrix
composition uses double intermediates and checks float representability before
narrowing. Original per-node transforms, including scale/mirrored facing, remain.

Rooted model matrices drive geometry, inverse-transpose normals, shadow model
constants and transparent world-depth order. The existing16-light selection is
unchanged in authored coordinates: proper rigid motion preserves its distance
ranking. Selected point-light positions follow the root; ranges/colors/powers
are unchanged. Extras keep their independent world transforms. All GPU data uses
existing copied constants and Context lifetime ownership; no shader, binding,
material64-byte ABI, render pass, point-light definition or private asset changes.

Two genuine native Reds are retained: geometry stayed at its old pixel, then
point-light instances failed to follow translated geometry. Greens verify the
new pixel and identity restoration, queued-view mutation behind a timeline gate,
invalid-root nonpublication, independent extras, co-translated lighting,
co-rotated normal/light pixels and reversed transparent depth order. Remaining
controls are characterization, not invented Reds. The first gate test lived past
explicit device close; Vulkan validation correctly reported its leaked semaphore
and aborted. A scope fix destroys the gate before close; the failure is retained
as a test-lifetime defect, not a renderer repair or waived validation message.

Six complete graphs pass69 selected/executed CTests: four inspector12/12,
shipping11/11 and shader-disabled10/10. Four serial native inspector GPU runs pass
with empty stderr in Clang debug/release/strict ASan+UBSan+leaks and GCC, followed
by four affected final reruns after the probe enum lint repair. Root success
markers cover rigid/copy/lights/extras/transparent order. These are correctness
runs, not optimized workload timing or scanout proof.

Native Ninja dependency metadata maps the changed header to35 actual Clang
invocations across inspector debug/release/ASan, shipping and shader-disabled
consumers. All pass after three affected probe reruns; no compile flags are
removed. Initial absolute-output metadata mapping and enum-size lint failures
are separated from behavioral Reds. Native GCC passes; its unchanged module
flags remain unsupported by Clang AST tooling. TSan's CPU-only graph has no
affected renderer TU. Source/format/whitespace and same-session Core/C++ review
cover transform frames, local bounds, copied constants, light ranking, sorting,
retirement and disabled-feature consumers; no remaining demonstrated P1/P2 was
found in this bridge. This is not independent review or exhaustive geometry,
compiler, driver or allocation-failure verification.

The preceding camera `c3805b6` passes hosted CI4/4, recorded separately. This root
source at `0fd447b` subsequently passes
[CI42949707939](https://github.com/kyaulabs/ngen/actions/runs/42949707939)4/4;
current Windows integration remains. Native text
normalizes trailing whitespace; ignored originals are intact. Live SDL input,
playable contact preset/renderer integration, force traces, audio, offline
packaging, warmed benchmarks, recording and control-feel feedback remain. M3 is
not complete; M2 appearance acceptance is not reopened.
