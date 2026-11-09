# Verified Windows process GPU memory

The owner returned archive SHA-256
`32f7620302ae380ecbf7f77c21cf7f8b87e8d4990c0448ac6b3f23312b4df548`.
It declares clean observer revision `5a2f8415aa226d9f757dfa18af403ab039d51806`
and reuses the exact original `b2fa7da` MSVC Release binary and CMake-cache hashes.
No rebuild, recook, provider installation or privilege change was requested.
Native JSON/CSV retain exact bytes; text normalizes paths/line endings. Bounded
archive admission and redacted secret scanning precede inspection. Session bytes
remain unread, capture pixels are neither extracted nor viewed, and the capture's
recorded artifact hash is verified without retaining its private bytes.

The actual RTX 5070 child exits zero after **3600 measured frames/60 warmup and
four completed reloads**. The wrapper now also succeeds in 27.580 seconds. All
recorded frames remain 1280x720, exposure 1/FXAA on/bloom .04, 1024 shadow,
246,080 triangles and 100 genuine asteroid instances; final generation is 5.
The canonical package and binary match the failed earlier memory run exactly.

| Observation | Verified result |
| --- | ---: |
| Retained query attempts | 50 |
| Positive own-process samples | 48, one adapter instance each |
| Empty startup attempts | 2, both outside the native measured interval |
| Missing measured-window observations | 0 |
| Largest measured observation gap | 605.3581 ms, below the 1000 ms bound |
| Peak dedicated GPU bytes | 250,916,864 |
| Peak shared GPU bytes | 69,296,128 |
| Peak **same-sample** dedicated + shared bytes | **304,472,064** |
| Peak total committed bytes, separate measure | **304,537,600** |
| Observed process working set/high-water bytes | **925,749,248** |

Every query/sample timestamp, its ordering, parent-QPC/native-frame correlation,
measured-window coverage and endpoint gap were checked independently of the
reported `complete` boolean. Peaks and each same-sample sum were recomputed.
Dedicated and shared independent maxima are **not** added as though simultaneous;
committed is **not** added a second time. Both sampled GPU measures pass 1 GiB;
process working set passes 6 GiB. This closes the missing native sampled WDDM
observation, not an exact continuous, global or indefinite-duration memory bound.
CIM counters are provider-refreshed; short transients may fall between samples.
Tracked VMA allocation/reservation peak at 448,107,760/451,045,936 bytes and
settle to 199,302,016/202,240,192 bytes with 22 live and zero retiring records
through the final 300 frames. VMA is not substituted for driver/WSI accounting.

All 3600 frame rows and recorded nearest-rank distributions were recomputed:
P50/P95/P99/max **7.1052/8.2455/8.5091/15.5172 ms**. Ordinary stderr is empty;
artifact hashes match. These instrumented memory-run timings do not replace the
separate clean performance runs. Reload-time C++/ImGui allocations remain visible.

Before this return, the owner confirmed that the CIM class and all required
fields exist, native GPU counter sets are present, and the exact two-second query
succeeds with zero rows for PowerShell's non-GPU PID. The earlier `CimException`
message was discarded by schema 2, so its cause remains **unknown**, not silently
called fixed or attributed to a timeout. The original failed archive is retained.
No driver/registry/SDK/execution-policy or persistent environment change was made
by the agent to obtain this result.

Scope: script execution is `5a2f841`, the reused compiled application is `b2fa7da`.
This does not execute the later compiled ship-bounds correction on NVIDIA. That
small remaining targeted Release check, final personal visual/input acceptance,
independent evidence follow-up and normal scoped PR integration remain separate.
M2 is not declared closed and M3 implementation has not started.
