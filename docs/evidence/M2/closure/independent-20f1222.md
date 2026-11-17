# Independent M2 review

Confirmed a clean, detached checkout at `20f12220166335c7d94f0ed6086171f6dbd00c1f`, tree `779c072fb4bb25862601f5dd505f0d3f3713f6f5`.

**No demonstrated production-code defect found in the inspected paths.** This is bounded independent agent advice. Human and owner approval remain outstanding.

## Findings and acceptance gaps

1. **P2 — The Linux resize latency failure remains unresolved.** Retained frame 44 took **256.820 ms**, including **254.597 ms** in presentation. Resize-adjacent acquisition polling is the bounded reproduction premise. Fixed-resolution runs and Windows results leave this failure unresolved. Resolve it or record an explicit acceptance disposition before closing resize-performance acceptance. [Acquisition path](../../../../engine/render/presentation.cpp#L293), [retained failure](../../../../docs/evidence/M2/bloom-workload/README.md#L72)

2. **P2 — Sorted translucent color compositing lacks a demonstrated production-path fixture.** `SceneRenderer` sorts blended draws back-to-front, but the inspected native controls establish masking, blended-caster exclusion and UI blending. The ship is entirely opaque. Add a checked-scene fixture with overlapping distinct-color translucent surfaces supplied in reversed order, plus opaque occlusion, and verify expected HDR composition and depth behavior. This is a coverage gap. [Sorting](../../../../engine/render/scene_draw.cpp#L280), [existing blend control](../../../../apps/resource_probe/main.cpp#L876)

3. **P2 — Whole-process NVIDIA graphics-memory acceptance remains unproven.** Windows VMA settlement and process working-set observations pass their recorded scopes. They leave the driver/WSI-inclusive graphics-memory cap unverified. Obtain scoped dedicated/shared GPU accounting or explicitly disposition that gate. [Measurement boundary](../../../../docs/evidence/M2/windows-workload-verified/README.md#L29)

## Evidence assessment

- **Fresh Iris Xe run.** All 41 recorded source hashes match this snapshot. Report, stdout, stderr and memory-file hashes match. Recomputed 3,600-frame P50/P95/P99/max are **16.745/26.736/33.715/47.270 ms**, entirely at 1280×720. Four reloads reach generation 5. Settled VMA allocation/reservation return to **211,566,728/214,396,216 bytes**, with 22 live and zero retiring records.
- **Memory scopes.** Fresh measured VMA allocation peaks at **471,943,656 bytes**. Sampled per-process i915 DRM peaks at **531,017,728 bytes**. Cold-inclusive Linux maximum RSS is **540,928 KiB**. Sampled RSS stays at **292,470,784 bytes** from frame 300 through completion. This supports bounded post-reload settlement over this run.
- **Windows.** Checked 30 selected retained artifact hashes, four 10/10 CTest summaries, native probe outputs and post reflections. Recomputed all four Release steady/stream CSVs. Their scoped budgets pass at `1f0a4b3`. Production source is unchanged through this snapshot.
- **Private captures.** Both show consistent lateral gutters, readable panel text, contained list clipping and localized red/cyan emission and bloom. No obvious panel overlap or bloom contamination of UI is visible. Hull detail is very dark and engine cores are saturated, leaving material-quality judgment for owner inspection. These representative frames leave other fixtures and displayed scanout unverified.

## Review scope and limitations

Read the requested eight renderer implementation/header pairs, relevant internal headers, Context, all three inspector/reload/UI pairs, three shaders, workload integration, checked-package admission, CMake graph, specified planning/evidence records and relevant ADRs. Selected probe/test regions covered gated retirement, copied borrowed inputs, generation-bound edits, worker cancellation/join, material controls, bloom, clipping and failure cleanup.

Tracing found successful-submit publication, completion-gated descriptor reuse, owning frame pins and separate presentation retirement consistent in those inspected paths. The shader GPU fixture remains a separate test implementation.

Tools used were read-only Git, shell text inspection/search and review-authored Python standard-library hash/CSV/JSON calculations. Builds, tests, sanitizers, AST analysis and benchmarks were **not run in this session**. Retained results were inspected. No files or remote state changed.

**Recommendation — keep formal M2 acceptance open** pending the findings above, owner visual/material acceptance, compiler-choice acceptance and the scoped PR Test Plan. The longer Iris run strengthens bounded memory evidence without creating an indefinite-duration requirement. Complete those M2 decisions before progressing to M3.
