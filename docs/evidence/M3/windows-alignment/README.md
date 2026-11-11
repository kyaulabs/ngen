# Explicit copied-view alignment

The second native MSVC return is clean `131bc55`, MSVC19.44.35229.0 and C++20.
Visual Studio reuse now reaches native compilation; the first failure occurs in
`scene_draw.hpp`: C4324 implicit alignment padding becomes an error under `/W4 /WX`.
This is a compiler compatibility failure, not a gameplay behavioral Red.

`SceneView` now names the existing8-byte gap between its borrowed material spans
and aligned root matrix. Native layout probes before/after agree: size288,
alignment16, factors184, flows200, root224, MaterialConstants64. Existing member
offsets and copied view size are unchanged. No warning suppression, shader binding,
material ABI, renderer ownership or physical behavior change is introduced.

The trusted Ninja build also collects independent translation-unit diagnostics
with `-k0`. Any error still returns nonzero through `Invoke-NgenNative` and prevents
CTest/acceptance from running. A real CMake/Ninja fixture runs both independent
failing targets and still fails, rather than converting failure to success.

Verification: ten relevant graphs/153CTests,74complete actual Clang invocations
selected by valid native header dependencies across eight configurations, both
PowerShell suites and pinned nonmutating formatting pass. Four native inspector
profiles preserve rigid roots/lights/transparency/extras and shared GUI contracts.
Four330-frame flight runs preserve finite rendering, bounded audio, real SDL/GUI
controls, tactical coast and reset orbits; all eight native stderr files are empty.
Strict ASan/UBSan/leak checks remain enabled. GCC uses its native build/tests, not
converted Clang commands. Raw artifacts remain in `build/m3-flight/alignment-*`.

Same-session source/C++ review finds no further actionable defect in this fix.
The public borrowed spans retain their lifetime; all known consumer commands in
these graphs are checked without filename-only deduplication or removed flags.
This is not independent review or native MSVC success: another owner rerun remains
required. The18-entry archive is preflighted and selected diagnostics are scanned;
full session/bootstrap and the now-successful dependency-build log stay unread.
No returned payload is executed. Original private asset/media bytes are untouched.
