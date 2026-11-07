# Queued inspector state and reload

**This is the inspector's command/ownership model and GPU material bridge, not
its runtime frontend.** The [static mockup](../../../mockups/README.md) remains a
visual study. Workload event/UI wiring, interactive reload/upload publication,
walkthrough and the final combined M2 gates remain open.

`ngen::tools::Inspector` owns a checked CPU package and scalar material factors.
Its 16-command FIFO accepts generation-bound selection, material, exposure/FXAA
and reload intents. Invalid values do not consume capacity. `advance()` publishes
changes at the next boundary; returned snapshots own their package and factor
values. Staged replacement resets asset selection/overrides, preserves view
preferences and rejects old-generation commands before applying them. It never
mutates a package or an old snapshot.

`PackageReloader` uses one dedicated `std::jthread` for blocking file I/O, outside
the simulation worker pool. There is one active/unconsumed request, explicit busy
backpressure, strictly increasing request IDs, cooperative cancellation and
stop/join before borrowed reader state is destroyed. A release/acquire completion
flag publishes an owning result. Exceptions become typed failure using a
preallocated fallback; no exception escapes the worker. Existing package limits
and cancellation do not make filesystem reads a sandbox or a hard deadline.

`SceneView::material_factors` is an optional complete material-indexed scalar
table. The production scene bridge validates and copies base color, metallic,
roughness and emissive strength, retaining authored texture/UV/sampler, alpha-mode,
cutoff and facing semantics. No tool header or GUI dependency enters that renderer
contract. Native pixels prove deferred edits, old-snapshot/source preservation,
three independent closed-form PBR cases and malformed table rejection.

| Actual verification | Result |
|---|---|
| Clang Debug/Release/ASan and GCC scene graphs | 12/12 CTests each; affected final worker reruns pass |
| Inspector/worker cases | 15 cases, 142 assertions; controlled cancellation, restart, shutdown, exception, capacity, generation and publication behavior |
| Separate headless TSan graph | 6/6, including real worker handshakes; affected final worker rerun passes |
| Native inspector material probe | Four compiler/configuration runs, ordinary validation and ASan/UBSan/leaks enabled where applicable |
| Existing full-package workloads/captures | 4/4 and 3/3 process checks per graph |
| Clang affected implementations/header consumers | 62 exact native invocations across enabled Debug/Release/ASan, headless TSan and disabled Release; eight repaired worker/test reruns complete that scope |
| Fresh tooling-disabled Release | 11/11; no inspector targets, translation units or linked `ngen::tools` symbols |
| Source style | Pinned clang-format 18.1.8 passes |

The first native material Red showed that a queued emissive-strength change did
not reach GPU pixels. CPU Reds separately cover deferred selection/material/view/
reload publication and worker admission. A later analytic probe initially used an
inverted front-face premise; correcting the test camera to the production Y-flip
made the closed-form cases valid without changing rendering semantics.

Actual AST findings are retained. Repairs remove an ineffective trivial move,
use a borrowed invocation-lifetime stop token, make integer widths explicit and
replace unnecessary optional thread storage with `std::jthread`'s empty state.
Test completion helpers unwrap results under explicit presence checks instead of
relying on assertion-macro flow analysis. No diagnostic was disabled.

Reproduce the model/native probe with prepared scene dependencies:

```sh
cmake --preset scene-debug -DNGEN_BUILD_INSPECTOR=ON
cmake --build --preset scene-debug
ctest --preset scene-debug
# Explicit physical-device check with the ordinary validation layer available:
build/scene-debug/inspector_probe
```

The default remains `NGEN_BUILD_INSPECTOR=OFF`. No new dependency is added in this
slice. The current disabled Release proof concerns this model/probe; repeat the
graph/link checks after adding the actual frontend. Later renderer/UI/bloom code
also needs new required-platform execution and performance/memory evidence.

Text paths/whitespace are normalized; original and retained hashes are separate.
`ast-results.json` records the final invocation-specific status, and
`compile-commands.json` retains flags with normalized paths. GCC AST limitations
and earlier asset vendor advisories are not reclassified by this evidence.
Same-session review is retained in `review.txt`, not independent acceptance.
