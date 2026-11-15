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

ClangCl Release compiles its translation units, then fails linking SDK imports:
`gdi32.lib`, `winspool.lib`, `shell32.lib`, `comdlg32.lib`, `imm32.lib`,
`setupapi.lib` and `pdh.lib`. No Release CTests, replay, memory or hover result
exists for ClangCl. The library-search failure is established; its underlying
environment cause is not. A local probe of the installed Conan 2.34.0 default
PowerShell environment round-trip preserved `LIB`, so that probe does not
reproduce or establish a Conan deactivation cause. Do not change renderer code,
SDK installations or global library settings from this evidence. Diagnose the
native search environment and retry the failed Release stage, not the successful
MSVC matrix.

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

Remaining: the ClangCl Release library-search failure and native Release gates,
two valid MSVC timing repeats, final recording/listening/offline-delivery and
owner acceptance, then reviewed integration and exact post-merge CI. The UI
starting point remains accepted with polish deferred. M3 is not complete.
