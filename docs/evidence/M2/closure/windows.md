# Targeted Windows M2 closure checks

Native transparency and corrected sampled WDDM accounting now pass. The reused
memory-test executable was compiled at `b2fa7da`, before the full-ship UI bounds
correction. **Only one updated MSVC Release build/native validation remains.**
Do not repeat the completed four-build matrix or run `-MeasureWorkload` again.
Slang, the specific box correction, final longer/wider blue-white exhaust and
remaining inspector/input behavior already have explicit owner acceptance. This
request is native execution evidence, not another personal approval. The new
private asset needs no Windows recook or offline toolkit.

Use the existing checkout updated to `origin/feat/kyau-ade38c-m2-acceptance`.
Do not reset owner changes. The exhaust follow-up changes asset data/receipts, not
the runtime/shaders or compiled source after `5a2f841`. Transfer the private
`build/ngen-m2-exhaust-input.zip` (47,526,154 bytes), SHA256
`1cd8b19aa17634129510be66aca4e4d7b2becee21d76bb9ea30aa60e0ee968a5`.
It contains only `scene.ngas`, its manifest and a data-only README; no reference,
brand bytes, executable or script. Extract to a **new separate directory**, such
as `build/m2-exhaust-windows-input`; keep the original `$scene`/canonical package.
The extracted package must hash to
`067407fb5fa2527771212cc7280a345b02ad3dd2a823ae8ba81b3553689ec52b`.

In the same MSYS2 fish terminal, once extracted:

```fish
set preview_scene (realpath build/m2-exhaust-windows-input/scene.ngas)
sha256sum $preview_scene
pwsh.exe -NoProfile -File (cygpath -w tools/build-windows.ps1) -Inspector -ScenePackage (cygpath -w $preview_scene) -Compiler MSVC -Configuration Release
```

The standard workflow builds the optional inspector graph, runs native fixtures
and ordinary validation, and retains automated walkthrough/captures. Expected
new markers in `Release-inspector.txt` are
`inspector_ship_bounds=1 inspector_rear_engine_bounds=1`, alongside
`scene_translucent_order=1 scene_opaque_occlusion=1 scene_translucent_depth_readonly=1`.
Retain the actual outcome, including any compile/validation failure. Send the
printed archive; there is no second memory command for this request. The new
workload should report 238,272 ship/plume + 8,000 asteroid = 246,272 triangles,
195 draw records, 14 materials, 5 images and all 612 retained light definitions.
The complete ship box should include the added plume bounds.

The owner has now explicitly answered **“Accept inspector/input.”** No extra
manual walkthrough, visual reapproval, compiler matrix or GPU-memory command is
required. If this exact updated MSVC Release/package run already exists, send its
archive rather than rerunning it. Newly observed concrete failures still need
investigation; automated execution is not a substitute for or reopening of the
recorded personal verdict.

Keep original model, package, captures, archives and session bytes private. No
persistent profile/environment, privilege, registry, driver, SDK, execution-policy
or signing change is requested. Returned evidence is selected and scanned before
inspection; it is never executed. Earlier failed GPU/resize evidence stays retained,
not rewritten as passing, and sampled memory is not an exact continuous/global peak.
