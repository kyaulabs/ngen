# Lattice continuity and R16F plateau correction

The fresh Windows archive SHA256
`096f9a7751e0cb8e73cb445f5d592c5dec484f4e2f891703b3216d8242c81d9f`
uses clean signed `b6ea825`. MSVC Release builds and10/10 CTests pass, then the
RTX5070 native resource probe exits1. Later native/final-package workloads do not
run. Twenty bounded native records are retained; five setup/session payloads
remain unread/unextracted. Original/path-normalized hashes are explicit in
`originals.json`. Returned code is never executed.

This time the failing sample is present. At pixel43,13/channel0, R16F words at
phase ticks−2/−1/0/+1/+2 are `38cb 38c9 38c6 38c6 38c4`. The wrap delta is
0.00146484375; the forward delta rounds to0. The old one-sided limit becomes
0.0009765625, despite both outer steps showing0.0009765625 ordinary motion.
Replaying those actual half values through the original predicate fails locally
with `smooth R16F flow plateau was rejected`. This is the behavioral Red, not a
compiler/setup failure. The earlier incomplete text/raw captures lacked the
sample; the raw executable marker was false and a split PowerShell redirection
command prevented complete capture. They do not establish a shader cause.

The revised detector decodes five finite HDR samples. It estimates ordinary
motion from the two outer steps, checks **both** central boundary steps, and
retains the exact multiplier2/absolute1/1024 allowance:

```text
normal   = max(abs(s[-1]-s[-2]), abs(s[2]-s[1]))
boundary = max(abs(s[0]-s[-1]), abs(s[1]-s[0]))
accept iff all five samples are finite and boundary <= 2*normal + 1/1024
```

The actual RTX plateau, reversed motion, zero and a half-exponent transition
pass. Left/right step seams, an isolated center spike and nonfinite values in
every sample position are rejected. Boundary steps cannot inflate their own
allowance, unlike the old forward-difference estimate.

The stronger check exposes a separate **real local spike** missed by the old
rule: at Iris pixel96,0, the +1 radiance is0.667480469; the five-sample forward
change is0.0151367188 while the outer-step limit is0.0107421875. Changing only
`frac(coordinate)` to `coordinate - float2(cell)` ties fractional interpolation
to the same floored lattice origin used for hashing. The native check passes;
restoring the original expression reproduces the exact same failing five half
words, and restoring the fix passes again. This is native A/B/A evidence, not a
claim about undocumented driver internals. `rtx-symmetric-green.*` names the first
attempted Green, which actually fails on this spike; it is not a passing result.
The double-precision calculation is
supporting arithmetic analysis only, not another GPU implementation or hardware
execution proof.

The shader arithmetic therefore **does change**. No noise weights, guide/tail
parameters, authored material/light/map/geometry, exposure/FXAA/bloom settings,
64-byte material ABI, bindings, Context owners, package format or dependencies
change. The final private package remains194,096,492bytes/SHA256
`a02d5438ad32d9c8fb917ce847d5a822688ca5b51d69125f6da71cf2ba2116ed`.
No claim of byte-identical pre/post-fix pixels is made. Visual verdicts stay
accepted; this is not a new art iteration.

A deliberate quarter-cycle phase seam still produces native exit1 and the
bounded failing sample. That mutation is removed before final verification.
Debug/Release/ASan+UBSan/GCC full graphs pass12/12 CTests each; shipping11/11 and
shader-disabled10/10,69 selected/executed tests total. Eight serial native
resource/inspector runs pass with empty stderr, including strict sanitizer/leak
settings. Five actual-command Clang analyses pass; GCC's previously unsupported
module flags are not stripped and no clean GCC AST result is claimed. No C++
header/ABI consumers change; the CPU-only TSan graph has no affected TU/shader.

Post-fix clean final-package runs retain all1,200 measured rows,600/60 per run:
steady P50/P95/P99/max16.483775/17.690003/18.446928/18.921454ms; mixed
16.542992/17.867540/37.239223/115.195218ms. Four reloads/two resizes/generation5,
247542triangles/211records and original budgets pass. Steady warm C++/ImGui calls
remain0. Validation is off for these timings; VMA settlement is not a new WDDM,
whole-process or continuous/global peak measurement. Private BMPs are excluded.

Same-session source review covers both changed files, the five-point calibration,
all finite-domain checks, bounded lattice conversions, disabled path and existing
shader consumers. No remaining concrete task-related defect is demonstrated by
local checks. This is assistant-operated, not independent human approval.
**Post-fix Windows execution is still pending.** Use the [corrected handoff](../windows-diagnostic.md),
then finish exact-head CI and normal reviewed integration before M3.
