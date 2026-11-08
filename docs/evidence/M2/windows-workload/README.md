# Windows inspector/bloom workload handoff

**Owner execution verified at `1f0a4b3`.** See the [returned results](../windows-workload-verified/README.md).
These commands are retained for reproduction, not a new four-build request.
This handoff covers the new inspector, shared
16-pixel UI gutters, restrained bloom and repeated full-package reloads on the
RTX 5070. It does not request another run at `ef984ef`; that earlier execution
slice is complete. Use the recorded revision below to reproduce this slice.

Run both compilers in Debug and Release using the same canonical `scene.ngas`
as before: 175,854,711 bytes, SHA-256
`e0ee6a023900c78b2c313f493f2a0e78c9639c39b6d830a1eaf3a3f6c0a29b6d`.
The original GLB/package stay private; this workflow does not recook them.

For MSYS2 fish, substitute the real package path:

```fish
git fetch origin
git switch --detach 1f0a4b31a3db73f802513eab6edebd319f3c47cb
set scene /path/to/scene.ngas
pwsh.exe -NoProfile -File (cygpath -w tools/build-windows.ps1) -Inspector -ScenePackage (cygpath -w $scene) -Compiler MSVC -Configuration All
pwsh.exe -NoProfile -File (cygpath -w tools/build-windows.ps1) -Inspector -ScenePackage (cygpath -w $scene) -Compiler ClangCl -Configuration All
```

Use the existing SDK/toolsets. The script selects the optional ImGui lock and
builds its SDL platform backend against the locked upstream Windows SDL. All
configuration stays process/run-local; it does not install drivers/SDKs, change
execution policy/registry/signing, overwrite a user Conan profile or relax warnings.
The pinned Python tooling environment remains under ignored `build/windows-tools`.

Both configurations run the existing CPU/native/shader/checked-package controls,
the actual inspector probe and a 1200-frame validation-enabled UI walkthrough
with two completed reloads and two verified resizes. Current inspector builds
normally enumerate ten Windows CTests; retain the actual listing/results rather
than substituting the old eight-test count.

Release additionally runs two 600-frame/60-warmup, validation-off workloads:
steady UI+bloom, then fixed-1280x720 streaming with four reloads and 30 settling
frames between them. Exposure/FXAA are restored after the edit demonstration;
bloom remains 0.04. Identity checks/earlier reads warm the file cache; these runs
do not establish cold-storage latency. Do not interact with automated benchmark windows. The known
resize-acquisition tail is recorded separately, not hidden in a relaxed budget.

The run directory is printed. Send the two generated `evidence.zip` archives,
even if a build/check fails; preserve the actual failed logs and avoid flag,
warning, SDK or driver workarounds. New evidence includes:

- revision/working tree, compiler/CRT identity, actual test listing and nine
  shader reflections (post constants 32 bytes; native PostDraw 80 bytes);
- `<Config>-inspector.txt` and validation walkthrough logs/capture/CSV;
- Release steady/stream logs, private captures, exact frame/memory/generation/
  extent CSVs and input/output SHA-256 records;
- per-child `*.working-set.json`, separate stderr and exit/deadline status.

The process observer uses argument arrays, samples OS working-set/high-water
counters and retains failure data. This is an observed process residency measure,
not continuous exact peak or GPU memory. VMA totals in native output are not
whole-NVIDIA/driver memory. Per-process dedicated/shared GPU-memory evidence
remains a distinct acceptance item; report unavailable counters honestly.

Captures/archives remain private. Account-bearing setup/session transcripts are
not rendering proof and will not be read as execution instructions. Returned
archives must be bounded/scanned before selected native logs are inspected.
Personal inspection/input feedback and independent/formal M2 acceptance remain
separate from automated success. In particular, note visual, input, clipping or
alignment defects rather than treating a green run as game-UI quality approval.

Local verification here is PowerShell syntax and real subprocess-boundary testing
on Linux, **not MSVC/clang-cl or RTX 5070 execution**. It covers output/input paths
with spaces, live process memory, failure/timeout propagation, missing captures/
reports, existing owner-file preservation, mode selection and environment cleanup.
