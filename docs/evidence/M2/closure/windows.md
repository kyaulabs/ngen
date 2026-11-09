# Targeted Windows M2 closure checks

Native transparency and corrected sampled WDDM accounting now pass. The reused
memory-test executable was compiled at `b2fa7da`, before the full-ship UI bounds
correction. **Only one updated MSVC Release build/native validation remains.**
Do not repeat the completed four-build matrix, recook/download the package or
run `-MeasureWorkload` again. Slang and the specific box correction already have
owner acceptance; remaining overall visual/input acceptance is separate.

Use the existing `5a2f8415aa226d9f757dfa18af403ab039d51806` source checkout and the
same existing `scene.ngas`. Later evidence-only commits do not change this source.
In the same MSYS2 fish terminal with `$scene` still set:

```fish
pwsh.exe -NoProfile -File (cygpath -w tools/build-windows.ps1) -Inspector -ScenePackage (cygpath -w $scene) -Compiler MSVC -Configuration Release
```

The standard workflow builds the optional inspector graph, runs native fixtures
and ordinary validation, and retains automated walkthrough/captures. Expected
new markers in `Release-inspector.txt` are
`inspector_ship_bounds=1 inspector_rear_engine_bounds=1`, alongside
`scene_translucent_order=1 scene_opaque_occlusion=1 scene_translucent_depth_readonly=1`.
Retain the actual outcome, including any compile/validation failure. Send the
printed archive; there is no second memory command for this request.

Personal visual/input review is distinct from automated execution. If not yet
checked, after automation closes open `pwsh.exe -NoProfile` with the new printed
Release directory and existing package:

```powershell
$release = (Resolve-Path 'build/windows/THE-NEW-PRINTED-RUN/Release').Path
$scene = 'C:\real\path\scene.ngas'
try {
    . (Join-Path $release 'conanrun.ps1')
    & (Join-Path $release 'scene_workload.exe') $scene --inspector --frames=3600 --warmup=0
} finally {
    . (Join-Path $release 'deactivate_conanrun.ps1')
}
```

Check the default ship/material/lighting presentation and panel gutters/list
clipping. Select parts, change material and render settings, and press R to reload.
Changes should appear at the next boundary without altering old/in-flight versions;
completed replacement resets asset selection/factors and preserves view settings.
The complete ship box must not shrink when part selection changes or clears.
Record remaining concrete defects or accept the visual/input slice. This does not
turn agent/GitHub account review into personal human approval.

Keep original model, package, captures, archives and session bytes private. No
persistent profile/environment, privilege, registry, driver, SDK, execution-policy
or signing change is requested. Returned evidence is selected and scanned before
inspection; it is never executed. Earlier failed GPU/resize evidence stays retained,
not rewritten as passing, and sampled memory is not an exact continuous/global peak.
