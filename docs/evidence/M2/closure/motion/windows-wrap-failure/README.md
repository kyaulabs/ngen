# Native NVIDIA phase-wrap failure

The updated Windows return uses clean signed source `062b4b9`. MSVC19.44 x64,
C++20/extensionsOFF and the dynamic CRT configure/build successfully; all10 CTests
pass. Native startup, validation callback and presentation checks pass before
`resource_probe.exe` exits1 with:

```text
Vulkan resource: emissive flow phase wrap is discontinuous
```

This is a genuine current-source native verification failure, not the earlier
old-source mismatch or a compiler/setup failure. The ordinary CTests do not
replace this GPU probe. The workflow stops there: subsequent shader/inspector/
final-package workloads did not run in this return. The last visible build line
is153/154; required links and subsequent execution pass without inventing154/154.

Archive SHA256 is
`9ae8922f1f8b13544ac27e14f893e7cb5e1777a3fe048d3cdfbe363d2126ee2d`,
18,483 bytes/25 bounded flat regular entries/58,537 uncompressed bytes. Twenty
native records are selected and redacted-scanned; five setup/session payloads
remain unread/unextracted. Returned code is never executed. Text paths/EOL and
terminal blank lines are normalized; `originals.json` distinguishes identities.

The old message contains no failing coordinate, raw half values or neighboring
radiances, so it does not distinguish a shader seam from a numeric-detector issue.
Do not guess the cause or relax the tolerance. The probe now records one bounded
failing pixel/channel and five samples at phase ticks −2, −1, 0, +1, +2. The
continuity predicate, half decoder, shader, renderer, workload and accepted art
are unchanged. This is diagnosis, **not a claimed NVIDIA fix**.

The added reporting is checked through the declared native graph: six affected
probe configurations build, four Iris Xe debug/release/sanitizer/GCC probes pass
with empty stderr, formatter/whitespace and five actual-command Clang analyses
pass. The shader-disabled variant compiles out this block. A deliberate local
readback jump still exits1 and produces the expected bounded raw/linear sample;
that mutation is removed before the final builds. No new matrix or performance
claim follows from these checks.

Next, update source and rebuild/run only `resource_probe` in the existing Windows
Release directory to collect the failing values. The [focused diagnostic](../windows-diagnostic.md)
does not repeat the full build matrix, asset cook, memory collection or personal
visual approvals. M2 remains open and M3 has not started.
