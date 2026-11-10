# Independent M2 follow-up review

Confirmed a clean, detached snapshot at `b2fa7da44d569cba2aec8221eac04b6df840b20a`, tree `8e1139df45b1d30cbeed8b144a1bb5df50dc15eb`. Its direct parent is `20f12220166335c7d94f0ed6086171f6dbd00c1f`.

## Actionable findings

1. **P2 — Incomplete GPU traces can pass.** After one valid, nonzero own-PID sample, subsequent empty or foreign-PID-only query results leave `$gpuStatus` as `observed` and disappear from the recorded samples. A successful workload can therefore receive `within_1gib_budget=true` while later reloads lack observations. Record unsuccessful query attempts and require bounded measured-window coverage, with explicit startup/exit handling. Add a valid-then-empty boundary case. This is a source-demonstrated control defect, without native Windows reproduction. [Collection](<ROOT>/build/m2-independent-review-tree/tools/build-windows.ps1:124), [final admission](<ROOT>/build/m2-independent-review-tree/tools/build-windows.ps1:191)

2. **P2 — The retained resize-performance failure remains open.** Fresh clean frame 24 takes **249.451 ms**, including **247.202 ms** presentation. Handoff variants still exceed 200 ms, and minimum-image-count execution reaches **204.368 ms**. The focused observation’s **16.685 ms median/105.328 ms maximum** supports a focus correlation only. Repair or explicit owner disposition remains required. [Failing frame](<ROOT>/build/m2-independent-review-tree/docs/evidence/M2/closure/resize/resize-clean-current.csv:26), [diagnostic assessment](<ROOT>/build/m2-independent-review-tree/docs/evidence/M2/closure/README.md:46)

## Coverage assessment

- **The public transparency controls substantively address the earlier gap.** Checked packages traverse real admission, `SceneUpload`, `SceneRenderer`, mesh pipelines and PBR shader execution. Independently calculated near-green-over-far-red HDR is `(0.25, 0.5, 0)`. Reversed composition produces `(0.5, 0.25, 0)` and fails. Opaque blue tests occlusion. Coplanar LESS cases require both colors and sum `.75`, detecting depth writes while permitting either tie order. Retained mutation logs fail at the intended assertions. Both production mutations are absent from the final diff. The attachment-only depth-readback failure is correctly classified as fixture setup. [Fixture](<ROOT>/build/m2-independent-review-tree/apps/inspector_probe/main.cpp:259)

- Retained results show **four passing native probes and 12/12 CTests per graph**, plus three passing Clang compilation-aware checks. GCC’s actual module flags remain preserved with its clang-tidy failure. The boundary Reds/Greens exercise real subprocesses and Linux CIM fixtures, including unavailable, malformed, entirely-zero, budget, clock and Measurement-mode cases.

- Windows source implements argument arrays, asynchronous streams, timeout/tree cleanup, explicit no-build dispatch, declared Release provenance, file/link/size admission, freshness checks, binary/package identity comparison and runtime cleanup. PID filtering, unique-instance aggregation, integer/capacity guards and separate dedicated/shared/same-sample-sum/committed metrics are present. Both aggregate budget checks use **1 GiB**. Finding 1 limits the completeness claim. Native provider behavior and returned-clock correlation remain unverified.

- Recomputed Linux results confirm **3,600 frames, four reloads**, fixed 1280×720 and **16.745/26.736/33.715/47.270 ms** P50/P95/P99/max. Final tracked allocation/reservation is **211,566,728/214,396,216 bytes**, with 22 live/zero retiring records. RSS is constant across frames 300–3599. This supports finite-run settlement.

## Verification scope and acceptance gaps

Inspected the three changed source/test files, relevant unchanged package/upload/draw/mesh/resource/shader/workload paths, closure diagnostics and retained logs, and acceptance/plan updates. Read-only Git/text inspection and review-authored Python standard-library calculations verified **101 retained-file hashes, three changed-source hashes, all 41 long-run source hashes and all ten resize distributions**. Original-byte hashes remain distinct from normalized retained hashes. Private binaries, packages and pixels were unavailable for independent revalidation.

No builds, tests, benchmarks, Windows/generated scripts or recorded commands were executed. Repository files and remote state were untouched.

**Formal M2 acceptance remains open** for the observer defect, native Windows WDDM/new-fixture execution, resize disposition, scoped PR Test Plan, and owner visual/material/compiler-choice judgments. This report supplies independent agent findings without asserting human acceptance.
