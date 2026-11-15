# Post-UI native Windows return

Source: signed `21e616c13c8a9344a84908a9b00bd53f1d35a8b8`. The owner reports a
direct desktop session. Four private archives represent one MSVC batch and one
ClangCl batch, each with a base archive and its supplement—not four independent
runs. `checks.json` records their hashes, bounded preflight and actual results.
Private archives, captures and raw logs remain outside Git.

MSVC 19.44.35229.0 passes Debug and Release; ClangCl 19.1.5 passes Debug: 24 CTests
per configuration, 72 total. Actual CMake toolchains use C++20 without extensions
and the dynamic Debug/Release CRT. The detected MSVC profile's default C++14 is
not the effective standard: the build explicitly overrides it to C++20.
Validated 330-frame SDL/control/audio walkthroughs, physical RCS geometry,
context-menu drawing and all GPU timing rows pass. All 27 physical fields agree
across these three configurations. The Release app-local package verifies and
starts with SDK paths removed. This is not proof of network disconnection.

In the initial return, ClangCl Release compiles its translation units, then fails linking SDK imports:
`gdi32.lib`, `winspool.lib`, `shell32.lib`, `comdlg32.lib`, `imm32.lib`,
`setupapi.lib` and `pdh.lib`. No Release CTests, replay, memory or hover result
exists for ClangCl at that stage. The library-search failure is established; its underlying
environment cause is not. A local probe of the installed Conan 2.34.0 default
PowerShell environment round-trip preserved `LIB`, so that probe does not
reproduce or establish a Conan deactivation cause. This evidence does not justify
renderer, SDK installation or global library changes, or repeating the successful
MSVC matrix.

The later native SDK audit, archive SHA-256
`5b8ec57c7a5d57def2f284052d34e53d81ed08766ed18500c624bb2dfbaf228b`,
starts in a fresh, uninitialized terminal. Developer-shell setup resolves all
six checked imports; native Conan activation/deactivation preserves their
library paths. The real `sim_harness` compile/link succeeds with exit code 0
and empty stderr. This establishes a working SDK context for cached Release
continuation, not the cause of the earlier failed session or completion of the
ClangCl Release runtime gates. See `sdk-audit.json`.

The later committed-script continuation at source `a4ebfc6` successfully links
ClangCl Release, including the native memory observer, and passes all 24 CTests.
Together with the earlier three configurations, 96 native CTests have passed at
their recorded revisions. Cached configure metadata is explicitly reused; MSVC
and ClangCl Debug are not repeated. The validated 330-frame walkthrough matches
the earlier MSVC result in all 27 physical fields. Ten declared artifact hashes
verify; app-local verification and SDK-isolated startup pass. Archive SHA-256 is
`e2900b0ca170ea37ff461af14b9289b5f147669e26b94a8bef022215ea2ed11a`:
51 bounded members, 1,584,222 compressed bytes and 34,094,594 expanded bytes.
Only admitted diagnostic text is extracted; no returned code or media is run.
See `clang-release.json`. Successful SDK initialization/linking does not establish
the cause of the earlier failure.

The new focused, audio-playing ClangCl clean replay retains all 2,400 frames.
Median/P95/P99/maximum are 6.9239 / 8.3083 / 8.6994 / 10.5208 ms, with zero warm
C++/ImGui calls. Separate process-memory observation has 32 valid samples from
33 attempts, one startup-empty attempt outside measurement, zero missing measured
queries and a 576.8024 ms maximum gap across 16.6763072 s. The maximum query is
228 ms; measured queries are at most 8 ms. QPC, all attempts, cadence, deadlines
and existing limits verify. Dedicated/shared peaks are 322,306,048 / 61,382,656
bytes; their peak sum is 383,688,704 bytes. Separate total committed peaks at
383,754,240 bytes; working-set high water is 439,857,152 bytes. Clean/memory
physical traces agree. This is one valid ClangCl run, not three MSVC runs or a
continuous hard peak guarantee.

The separate 3,600-frame live-panel run stays focused and playing. Flight Data
and numeric F12 draw for 3,386 and 3,337 frames; 24 OS samples complete without
failure. However, it records **one warm ImGui allocation**, with zero warm C++
calls. Its allocation-free gate fails despite exit code 0. The archive does not
identify that allocation's call site.

A local 32-motion-event plot-hover regression separately reproduces eight warm
allocations. Late-enabled stack tracing identifies ImGui input queue/trail growth.
GUI-only motion coalescing now retains each run's first event, first distinct
pixel and final position, flushing at other events, device/window changes and
frame begin. Flight input still receives every SDL event; no resource capacity,
warmup, detector, layout or physics limit changes. Independent review reproduced
a returning fractional-motion source transition that the first coalescing attempt
lost; a public source regression verifies its repair. All five 2,400-frame hover,
burst, button-order and source cases pass with zero warm calls. Nineteen UI cases
pass after initial coalescing, with six affected gesture cases rerun after the
review repair. Four graphs pass 104 CTests and three actual-command GUI Clang
analyses. Four native sanitizer UI cases, two inspector probes and a sequential
strict ASan/UBSan 330-frame walkthrough pass; all 27 physical fields remain
unchanged. An earlier concurrent walkthrough lost focus and failed its relative-
capture fixture; that attempt is retained, not accepted. The fresh read-only
follow-up review finds no remaining concrete defect in this scope. These local
results do not attribute or waive the native single allocation: Windows live
hover after the repair remains required.

All MSVC timing rows are retained, including focus transitions and the long tail:

| Replay | Median ms | P95 ms | P99 ms | Maximum ms | Unfocused frames | Valid active profile |
|---|---:|---:|---:|---:|---:|---|
| Base | 7.12535 | 7.8040 | 8.3627 | 9.6179 | 0 | Yes |
| Additional 2 | 7.14400 | 7.9814 | 9.1673 | 579.3141 | 1,448 | No |
| Additional 3 | 7.12975 | 7.9599 | 8.7390 | 23.8833 | 755 | No |

Each replay contains 2,400 frames at 1920 × 1080 after 60 warmup frames, with
validation and external process-memory sampling off, hidden F12, and zero warm
C++/ImGui allocation calls. Every P95 is below 16.7 ms, but only the base replay
keeps focus and playing audio throughout. The owner says multitasking may have
changed focus. **Three-valid-run RTX acceptance remains open.** Repeat only the
two affected clean replays using the existing immutable MSVC bundle. Do not
remove unfocused rows or the 579.3141 ms frame to manufacture valid results.
Earlier Artemis-session misses remain historical evidence, not retroactively
waived or causally attributed by this direct-desktop result.

Separate MSVC process-memory observation passes the existing limits: 33 valid
samples, 34 attempts, one startup-empty attempt outside the measured window,
zero missing measured queries, and a 601.1633 ms maximum gap over 16.6750195 s.
The maximum query takes 253 ms; measured-window queries take at most 3 ms. QPC
containment, every attempt, 500 ms target cadence, 2 s request deadline and 1 s
maximum measured gap verify. Dedicated/shared process peaks are
313,434,112 / 53,088,256 bytes; their peak sum is 366,522,368 bytes. Separate total
committed peaks at 366,587,904 bytes. Working-set high water is 439,074,816 bytes.
These pass the 1 GiB process-GPU and 6 GiB process-working-set limits; sampled
coverage is not a continuous hard peak guarantee. VMA and whole-device readings
remain separate. The clean and memory runs retain identical 27-field physical
traces. Eighteen declared capture/report hashes verify.

The 3,600-frame MSVC live diagnostic keeps focus and playing audio on every row,
with zero warm C++/ImGui calls. Flight Data draws for 3,391 frames; F12 draws for
3,301 frames and its OS worker publishes 23 samples without a collection failure.
The owner confirms actual F2 plot tooltips. F12 is a numeric telemetry panel,
not a plot panel: the handoff's request to find F12 plots was incorrect. The
verified scenario is F2 plot hover while F12 remains live; no UI feature is added.
The short MSVC Release walkthrough ends with CPU availability false, honestly
unavailable rather than a valid zero; its later live diagnostic has an available
CPU reading. The other successful walkthroughs have CPU/RAM/device readings.

The same production source's Linux v22 bundle also passes isolated startup and
three focused, audio-playing B1 runs; their P95 values are 17.274232, 17.138843 and
17.245040 ms. Earlier Linux focus-invalid trials are retained separately, not
accepted. The post-review bundle is
`build/m3-offline-linux-v22/ngen-flight/run.sh`; its manifest SHA-256 is
`ce0dd77a0760e8abaee7ed30ba475dac9773efa15cee966f23c86db54c009881`.

Remaining: native allocation-free live hover after the GUI repair, two valid
MSVC timing repeats, final recording/listening/offline-delivery and
owner acceptance, then reviewed integration and exact post-merge CI. The UI
starting point remains accepted with polish deferred. M3 is not complete.
