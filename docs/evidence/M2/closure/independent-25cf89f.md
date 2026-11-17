Confirmed clean, detached commit `25cf89ff8a4db4454b7adff895f2e6a5f494260a`, tree `557e0c41745256f7be32e4a8dd6c985165f4631e`. `5a2f841..HEAD` changes only evidence/documentation; no production source or dependencies changed.

**One demonstrated P3 reporting defect; no demonstrated P1/P2 in scope.**

- **P3 — Settings invariance is overstated:** [memory README:15](<ROOT>/docs/evidence/M2/closure/windows-b2/memory-verified/README.md:15) says every frame uses exposure 1/FXAA on. The unchanged reused workload deliberately renders measured frames **1–2** at exposure **.75/FXAA off**, restoring settings at frame 3 ([source:428](<ROOT>/apps/scene_workload/main.cpp:428)). Consequence: describe these as restored/default settings, with the brief scripted edit. This does not invalidate the sampled-memory result.

**Scoped verdict: the native sampled accounting closes the prior missing-evidence gap.** Independent inspection/calculations confirmed:

- Declared clean script revision `5a2f841`; binary, cache and package hashes exactly match the earlier failed observation.
- Child exit zero, reported 60 warmup frames, **3,600 valid measured rows**, fixed 1280×720 and four completed reloads. Generation transitions occur at frames **63/145/225/307**, ending at 5.
- Nearest-rank frame P50/P95/P99/max: **7.1052/8.2455/8.5091/15.5172 ms**. Other recorded timing distributions also match.
- **50 attempts, 48 positive samples**, one instance each. Both empty attempts precede measurement. Native/parent QPC correlation and ordering match; 46 positive samples fall inside measurement. Endpoint gaps are **57.6575/495.3198 ms**; maximum gap **605.3581 ms**, below one second.
- Independent dedicated/shared maxima: **250,916,864/69,296,128 bytes**. Maximum actual same-sample sum: **304,472,064 bytes**. Separate committed maximum: **304,537,600 bytes**. Both pass 1 GiB; committed is not added again. Recorded working-set/high-water accounting reconciles to **925,749,248 bytes**, below 6 GiB.
- VMA peaks **448,107,760/451,045,936**, settling to **199,302,016/202,240,192 bytes**, with **22 live/0 retiring** throughout the final 300 rows.
- Selected retained hashes match. Native JSON/CSV match recorded originals and Git bytes under the narrow attributes; normalized text/revision hashes reconcile through CRLF restoration. Historical independent review remains unchanged and revision-scoped.

Caveats are separate from demonstrated defects:

- [Memory scope:37](<ROOT>/docs/evidence/M2/closure/windows-b2/memory-verified/README.md:37): provider-refreshed process accounting cannot prove continuous/global peaks or indefinite stability. Instrumented timings do not replace clean performance evidence.
- [Provider record:14](<ROOT>/docs/evidence/M2/closure/windows-b2/memory-verified/provider-checks.txt:14): subsequent queries succeed, but the original discarded `CimException` cause remains unknown.
- [Remaining validation:3](<ROOT>/docs/evidence/M2/closure/windows.md:3): the reused binary does not execute the newer ship-bounds correction. One updated MSVC Release validation, final personal visual/input acceptance and normal PR integration remain open. **M2 remains open; M3 has not started.**

Only read-only Git/source inspection and inline calculations were performed. No builds/tests/benchmarks, edits, remote access or host changes occurred. Capture hash validation was inspected as recorded metadata; pixels, sessions and archive bytes were excluded. This is agent advice, not human/owner approval.