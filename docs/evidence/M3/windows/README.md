# Windows flight baseline and ClangCl CRT repair

The owner returned two archives at clean `9f5dbbf`. MSVC 194 passes 19/19 CTests in
Debug and Release. Both 330-frame validation/control/audio traces pass the existing
SDL, bounded-audio, manual/tactical coast and reset checkers. The accepted package
SHA-256 remains `a02d5438ad32d9c8fb917ce847d5a822688ca5b51d69125f6da71cf2ba2116ed`.

One clean RTX 5070 replay renders 2,400 frames at 1920 × 1080: median 7.21795 ms,
nearest-rank P95 7.7425 ms, P99 8.7533 ms and maximum 9.4156 ms. Every GPU row is
ready, all rows are focused with audio playing, and measured C++/ImGui allocations
are 0. All six Release artifact sizes/hashes match. Private BMPs are streamed only for
integrity/header checks, never extracted, viewed or committed. Three rendered
app-local frames and headless admission pass without SDK paths; the owner also
confirms actual disconnected-network interactive startup works.

The separate instrumented replay has 2,400 correlated native-frame rows and 31
successful WDDM observations/attempts. Maximum observation gap 567.7227 ms and
maximum query 1,945 ms satisfy the existing gap/deadline bounds; no measured query
is missing or exempted. Observed process working-set peak 433,311,744 bytes,
per-process dedicated + shared GPU peak 379,162,624 bytes and committed peak
379,228,160 bytes fit their budgets. These are sampled/provider-refreshed process
observations, not exact
continuous peaks or global GPU use; do not add them to VMA or working set.
Clean and instrumented physical traces match. This is one clean baseline run,
not three-run RTX variance evidence or final acceptance after upcoming RCS/UI work.

ClangCl 19.1.5 compiles all owned translation units but the linker correctly rejects
Jolt's MTd objects against the application's MDd objects. Conan's `is_msvc` helper
excludes ClangCl, leaving Jolt's static-CRT default selected. The recipe now follows
the declared `compiler.runtime` for either frontend; it does not bypass mismatch
checks or patch upstream physics. Only Jolt entries change in the physics/demo
locks, to recipe revision `67a3b4c0d2e06dc7514a970066ac5074`.

A real Conan generator regression fails for all four ClangCl variants before the
repair and passes all eight frontend/build-type/runtime variants afterward. CI
runs it without claiming Windows compilation. The new pinned recipe passes eight
native Linux graphs with 116 CTests and four 330-frame SDL/GUI/audio/sanitizer checks.
No owned C++ source/header changes here, no new dependency or warning suppression.
Same-session source/build/ownership review finds no further actionable defect;
it is not independent review. A new native ClangCl link/run remains required.

Archive metadata is bounded/preflighted and selected diagnostics pass redacted
secret scans. Broad telemetry selection hit the safety guard; only explicit
smaller diagnostic selections followed, without changing the guard. Session,
bootstrap and dependency-build payloads stay unread; no returned code executes.
M3 remains open, including the requested RCS visuals and later EVE-referenced UI pass.
