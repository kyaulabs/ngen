# M2 closure verification

**M2 remains open. M3 has not started.** PR #8 integrated the implementation at
`20f1222`; integration is not milestone acceptance. This follow-up obtains a
separate read-only agent review, closes its checked-scene transparency coverage
gap, extends the measured reload run and verifies native Windows GPU accounting.
No production renderer/shader source or dependency changes in the initial slice.

Owner follow-up: Slang 2026.21 and the specific rear-engine
[ship-bounds correction](ship-bounds/README.md) are accepted. After owner-reported
thermal/CPU-power changes, [current-host verification](current-host/README.md)
passes a fresh clean 3,600-frame mixed run under the original timing bounds;
old failures remain retained, without an engine-repair or exact-cause claim.
The separate review at `b2fa7da` confirms transparency coverage but finds an
observer gap. [Corrected coverage/deadline controls](observer-corrections/README.md)
pass public-process boundary tests. A [fresh independent follow-up](independent-b92ee53.md)
confirms clean `b92ee53` and finds the prior P2 substantively addressed, with no
additional demonstrated P1/P2 defect in scope. Its duplicate-test overstatement
and stale nested resize wording advisories are corrected; this is agent advice,
not owner approval or native WDDM execution.
The [returned MSVC Release run](windows-b2/README.md) passes the new NVIDIA
transparency controls. Its first memory wrapper fails honestly and stays retained.
The [corrected native observation](windows-b2/memory-verified/README.md) now verifies
48 positive samples, no measured-window gaps and both sampled GPU budgets using
the same binary/package. [Independent data follow-up](independent-25cf89f.md)
confirms the sampled-accounting gap is closed; its one P3 settings-wording finding
is corrected without changing code or dropping frames. The original CIM failure's
cause is still unknown.

The owner now separately accepts the [blue-white exhaust presentation](exhaust/README.md),
including localized light spill and the explicitly requested longer/wider plume.
The original GLB/canonical package remain unchanged. The exact accepted private
variant reproduces byte-for-byte, preserves original records and adds 192 visible
triangles through existing transparent rendering. New clean 600-frame steady/mixed
runs pass original timing budgets; steady warm C++/ImGui allocations are zero.
This is not overall inspector/input acceptance or a new WDDM measurement. One
updated MSVC Release check now uses that separate package; no compiler matrix or
memory observation repeat is requested. The [independent exhaust follow-up](independent-afb94f7.md)
confirms clean `afb94f7`, recomputes all 1,200 rows and finds no actionable defect
or new advisory in the offline receipt/evidence scope. It did not execute retained
scripts or inspect private artifact bytes and supplies no personal human approval.

The root source/verification records below describe the initial `b2fa7da` slice;
ship-bounds and exhaust records carry their own changed-source/artifact identities.

The independent review confirmed clean `20f12220166335c7d94f0ed6086171f6dbd00c1f`
and tree `779c072fb4bb25862601f5dd505f0d3f3713f6f5`. It found no demonstrated
production-code defect in the inspected paths, but identified missing overlapping
translucent composition coverage, unresolved Linux resize latency and missing
whole-process NVIDIA graphics-memory evidence. It inspected actual private Iris
Xe/RTX captures, source contracts, hashes and numeric traces. Its report is agent
advice, not human/owner approval, and explicitly records that it did not run builds,
AST analysis, sanitizers or benchmarks. Session/account logs and pixels stay private.

The new `inspector_probe` fixture loads checked packages with far red/near green
half-alpha quads and an intervening input-order opaque blue occluder. It verifies
linear HDR `(0.25, 0.5, 0)` outside opaque coverage, blue under opaque coverage,
and byte-identical pixels after reversing instance order. Coplanar variants require
both colors through LESS depth testing without depending on a sorting tie-breaker;
this proves transparent depth is read-only. Deliberately reversed sorting fails
at composition; deliberately enabled transparent depth writes fail at the
coplanar case. Both mutations are removed. The first fixture attempt incorrectly
requested readback from an attachment-only terminal depth layout; that is retained
as a fixture setup failure, not a production defect or behavioral TDD Red.

All four Linux inspector graphs pass **12/12 CTests** and the actual native probe
under ordinary validation. Clang Debug/Release/ASan+UBSan/leaks and GCC Debug use
the same public production renderer. Three actual Clang AST invocations pass
after correcting two test-only multiplication-width findings. GCC builds/runs,
but clang-tidy rejects its actual module flags; those flags are retained rather
than stripped, and no GCC AST-clean claim is made.

The longer unchanged-production Iris run has 60 warmup/3,600 measured frames,
four completed reloads and 66.198 seconds including startup. P50/P95/P99/max are
**16.745/26.736/33.715/47.270 ms**, with every extent 1280x720 and bloom .04.
Exposure 1/FXAA on are the default/restored settings; the scripted edit uses
exposure .75/FXAA off for frames 1–2 and restores at frame 3, omitting no frames.
All scoped budgets pass. Tracked allocation/reservation settle
to 211,566,728/214,396,216 bytes, 22 live/zero retiring records. Sampled process DRM
peaks at 531,017,728 bytes; cold-inclusive Linux peak RSS is 540,928 KiB. The
independent reviewer verified all 41 source hashes and recomputed the distributions;
sampled RSS is 292,470,784 bytes from frame 300 through completion. This supports
bounded settlement over this run, not indefinite-duration or cold-storage proof.

Resize investigation retains all failures and counterexamples. A predeclared
three-pair native-boundary experiment compared ordinary teardown with retaining
`oldSwapchain` until replacement creation. One aggregate 200-second diagnostic
collector deadline interrupted the final planned case; its partial files remain,
and that unfinished case was completed once under fresh names. Both variants
still exceed 200 ms, so no swapchain-handoff production patch is promoted. A
minimum-image control requests/receives three instead of four FIFO images but
still reaches 204.368 ms; it is not a fix either.

A fresh **uninstrumented** mixed resize/reload run records P50 50.619 ms and
249.451 ms maximum, including 247.202 ms presentation at frame 24. Separate
instrumented observations show a visible, non-minimized, non-occluded but unfocused
window on a 60 Hz KDE
Wayland display. A later observation sees focus become active and records
16.685/20.260/35.383/105.328 ms P50/P95/P99/max with unchanged production code.
Both parent/native thread timer slack are 50,000 ns and observed 1 ms sleeps are
about 1.06 ms. This narrows the issue toward focus/compositor-sensitive pacing;
it does not prove the exact external cause, repair the original failure, or
silently waive a budget. Instrumented observations are not clean performance
passes. Subsequent owner-assisted controls under the reported new thermal/power
policy confirm that focus alone does not reproduce the failure. The later clean
3,600-frame mixed run passes unchanged budgets, maximum 103.571 ms; this is a new
host-condition result, not a waiver or retrospective repair of the old dataset.

`tools/build-windows.ps1 -MeasureWorkload <existing-Release-directory>
-ScenePackage <same-package>` now runs 3,600 measured frames/four reloads without
bootstrapping compilers, rebuilding or installing anything. It keeps the original
build record, hashes the binary/package, restores the existing Conan runtime and
retains fresh private capture/CSV and parent observations. Local CIM/WDDM counters
are filtered to the owned PID and summed once per adapter instance. Dedicated,
shared, their same-sample sum and total committed are separate fields; the last
quantity is not added again. Both observed sums must stay within 1 GiB. Unsupported,
malformed, duplicate, missing or entirely-zero GPU-workload data cannot become a
passing zero-memory result. Parent Stopwatch/QPC window/frequency and sample ticks
permit returned-clock validation rather than assuming native correlation.

The process tests are **Linux PowerShell boundary verification**, not Windows
execution. They use real child processes and mock only the CIM boundary. They
exercise foreign PID isolation, unavailable/malformed/zero data, both GPU budget
failures, monotonic observation windows, spaced paths, native/absent-output failure,
existing owner-file preservation, Debug/dirty-build rejection and environment
cleanup. Native schema-3 execution is now separately verified: 50 query attempts,
48 positive samples, two empty startup attempts outside the measured interval and
a 605.3581 ms maximum gap. Peak same-sample dedicated+shared is 304,472,064 bytes;
committed is separately 304,537,600 bytes; process working set peaks at 925,749,248
bytes. Provider-refreshed/sampled accounting is not global usage or a hard
continuous peak. CIM calls
can delay the nominal working-set cadence; actual GPU sample/query times remain
visible. The instrumented memory run is not substituted for clean timing results.

Next: verify the compiled ship-bounds correction and accepted exhaust package in
one targeted Windows Release run, obtain remaining inspector/input acceptance and
complete normal scoped PR integration. No further memory observation or duplicate four-build matrix is
requested. Independent follow-up for the current corrections is
complete, with its two evidence advisories corrected.
Only then close applicable M2 gates and start M3. Slang and the specific box fix
already have owner verdicts; do not request them again. The completed four-build
production matrix needs no duplicate run, and the new transparency control now
passes MSVC Release at `b2fa7da`. Schema-3 native observation now passes at
`5a2f841` using that exact binary. The latest compiled UI bounds still needs its
separate targeted native verification.

Original and retained hashes are separate. Text/CSV line endings, ANSI diagnostics
and displayed repository paths are normalized; original metadata artifact hashes
refer to the private original bytes. The nested `windows-b2` records instead
preserve native JSON/CSV exactly under narrowly scoped attributes. `files.sha256`
covers the retained forms.
Actual compile arguments preserve all configuration-specific flags, including GCC
module flags. Private model/package/capture/archive and agent session bytes are
not committed.
