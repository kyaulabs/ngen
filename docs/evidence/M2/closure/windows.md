# Targeted Windows M2 closure checks

Use the supplied pinned closure revision and the **same existing `scene.ngas`**.
No recook or package download is needed. The completed MSVC/clang-cl Debug/Release
production matrix remains valid: production renderer/shader source is unchanged.
This request adds one updated Release transparency control and the missing
per-process graphics-memory observation, **not another four-build matrix**.

In MSYS2 fish:

```fish
git fetch origin
git switch --detach <supplied-full-closure-revision>
set scene /real/path/to/the/existing/scene.ngas
pwsh.exe -NoProfile -File (cygpath -w tools/build-windows.ps1) -Inspector -ScenePackage (cygpath -w $scene) -Compiler MSVC -Configuration Release
```

That one Release run compiles the new checked-scene translucent fixture and runs
the existing native controls. Look for
`scene_translucent_order=1 scene_opaque_occlusion=1 scene_translucent_depth_readonly=1`
in `Release-inspector.txt`; preserve the actual outcome, including failures.

Use its printed run directory for the next command:

```fish
set release build/windows/THE-PRINTED-RUN/Release
pwsh.exe -NoProfile -File (cygpath -w tools/build-windows.ps1) -MeasureWorkload (cygpath -w $release) -ScenePackage (cygpath -w $scene)
```

The second command **reuses that binary without building or installing anything**.
For an earlier memory-only check it can also reuse the already verified
`build/windows/20261107-193638-927/Release` MSVC build, if that directory still
exists. It requires the existing CMake Release cache, original adjacent
`logs/revision.txt`, empty original `logs/working-tree.txt` and generated Conan
runtime scripts. Those records declare the build provenance; their presence alone
is not cryptographic proof of binary/source equivalence. Binary/package hashes
are retained before/after observation.

Leave both automated workloads alone. The memory run uses 60 warmup and 3,600
measured frames, four completed reloads, fixed 1280x720, exposure 1, FXAA on and
bloom .04. It queries local WDDM/CIM per-process counters at a nominal 500 ms
cadence; actual query/sample times and parent clock window are retained. This is
instrumented memory evidence, not a replacement clean performance measurement.
Working-set polling has a nominal 25 ms wait plus CIM overhead. Observations are
sampled/provider-refreshed, not exact continuous peaks or global device usage.

Send **both printed `evidence.zip` files**, including failures. The second is under
`build/windows-measure/` and includes `long-stream.{txt,csv,bmp,working-set.json,
input.json,artifacts.json}` plus separate stderr and script revision/failure data.
Unsupported/missing/malformed/entirely-zero counters fail honestly; do not install
providers, change privileges, registry, driver, SDK, execution policy or signing
to force a pass. No API key or persistent profile change is needed. We will select
and scan native results before inspecting them; setup/session logs stay private.

Personal visual/input review is separate. After automated runs have closed, open
`pwsh.exe -NoProfile` and use the real paths in this optional manual session:

```powershell
$release = (Resolve-Path 'build/windows/THE-PRINTED-RUN/Release').Path
$scene = 'C:\real\path\scene.ngas'
try {
    . (Join-Path $release 'conanrun.ps1')
    & (Join-Path $release 'scene_workload.exe') $scene --inspector --frames=3600 --warmup=0
} finally {
    . (Join-Path $release 'deactivate_conanrun.ps1')
}
```

Check the default ship/material/lighting presentation, list clipping and shared
side gutters; select instances, edit factors/view settings and press R to reload.
The manual run ends after its bounded frame count; it can be relaunched for
inspection. Closing it early can report cancellation and is not a failed automated
benchmark. Report actual defects or an explicit personal acceptance verdict; an
automated green exit is not personal visual approval. The independent reviewer
found the hull very dark and engine cores saturated, leaving that quality judgment
to the owner rather than inventing a renderer defect.
