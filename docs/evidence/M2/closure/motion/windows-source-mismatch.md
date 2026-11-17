# Final package returned with old Windows source

The supplied archive is 602,526 bytes, SHA256
`5b4f45a0b146ca7e6f18261708635583f07b73a472ada1a71069d90d9b238c8e`.
Preflight admits 58 flat regular entries/15,688,237 uncompressed bytes with bounded
names, sizes, compression, duplicates, types and encryption. Selective extraction
keeps 53 native records/captures; five setup/session logs remain unread and
unextracted. The redacted selected-file scan passes. Returned code is not executed
and private capture bytes are not committed.

The input is the correct final package, 194,096,492 bytes, SHA256
`a02d5438ad32d9c8fb917ce847d5a822688ca5b51d69125f6da71cf2ba2116ed`.
Native package readbacks report 17 materials/nine images, 106,343,076 mip bytes,
210 scene draws and 239,542 ship triangles. The workload includes 100 asteroids,
247,542 total triangles and 211 records. This verifies final geometry/material
execution on RTX5070 with the previous renderer.

**It does not execute the new flow renderer.** `revision.txt` records clean
`5a2f8415aa226d9f757dfa18af403ab039d51806`, not renderer `659d570` or its later
evidence tip. The shader manifest is byte-identical to the earlier `5a2f841`
return, resource/inspector flow markers are absent, and every CSV still has
18 columns without `emissive_flow_phase`. All eight changed C++/shader paths are
missing from that old revision. A fresh build of old source is not a current
renderer build; this is a source-scope mismatch, not a demonstrated engine defect.

Useful checks pass within that scope: MSVC Release10/10 CTests, native bounds and
transparency controls, complete asset readbacks and empty workload validation
stderr. All 2,400 measured rows, native clocks, distributions, extents/generations,
artifact hashes and last300-frame settlement are independently recomputed.

| Run, 60 warmup | Frames | P50 / P95 / P99 / max ms |
|---|---:|---|
| Validated inspection | 1,200 | 14.574 / 21.838 / 22.340 / 31.880 |
| Clean steady | 600 | 14.672 / 21.661 / 22.133 / 22.307 |
| Clean fixed stream | 600 | 14.635 / 21.608 / 22.115 / 22.339 |

Clean timing meets original budgets and steady warm C++/ImGui calls are zero.
Working-set peaks are 865,841,152/401,522,688/973,107,200 bytes, below6GiB. These
are not new-shader timing, motion, WDDM or continuous/global peak proofs. Windows
floating-fixture CRC `3f1c5f23` differs legitimately from Linux; package CRC is
`a2eb7c22`. The private native inspector capture was viewed without requesting
another personal verdict.

M2 remains open. Keep this correct asset; update the Windows work branch, verify
that signed renderer `659d570` is an ancestor of HEAD **before** building, then
run the single fresh MSVC Release workflow. Do not recook, rerun an old matrix,
collect WDDM again or reopen accepted appearance. The [current handoff](windows.md)
includes a fail-closed source check to avoid another old-source run.
