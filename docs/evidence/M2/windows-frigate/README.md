# Full-frigate Windows validation

This is a **new execution handoff**, not a retained NVIDIA success or final M2 acceptance. It covers generated IBL, real directional shadows and the complete checked frigate, beyond the [successful earlier Debug fixture](../windows-scene-debug/README.md). No new SDK, driver or persistent environment setup is requested.

Copy the private development input into `build/windows-frigate-input/` on Windows: `scene.ngas` and its existing cook `manifest.json`. A local transfer archive is prepared at `build/windows-frigate-input.zip`; extract it into the checkout's `build/` directory. It contains the cooked package, manifest, checksums and private-use notice, **not** the original GLB. The prepared ZIP is 47,520,371 bytes, SHA256 `6065233462583b2691e236922adb0c097d0df252c1872bd2fe0c089a6e131c54`. Do not commit or publicly upload the archive/package/captures: redistribution is not approved.

The package SHA256 must be `e0ee6a023900c78b2c313f493f2a0e78c9639c39b6d830a1eaf3a3f6c0a29b6d`. The runtime remains importer-free; Windows does not need Slang/KTX/importer source tooling at runtime or a new tangent rebuild. `-ScenePackage` implies `-Scene` and the renderer/shader graph.

From MSYS2 fish, after copying the input:

```fish
cd /d/Git/kyaulabs/ngen
and git fetch origin
and git switch feat/kyau-e39ea3-scene-frames
and git pull --ff-only
and begin
    set package (cygpath -w "$PWD/build/windows-frigate-input/scene.ngas")
    for compiler in MSVC ClangCl
        pwsh.exe -NoProfile -File (cygpath -w "$PWD/tools/build-windows.ps1") -ScenePackage "$package" -Compiler $compiler
        or break
    end
end
```

Each compiler runs **Debug and Release** for the new combined source. This is not a request to reconfirm the old padding/narrowing fixes: it adds later environment/shadow code and actual full-package execution. To isolate Release first, append `-Configuration Release`; that result would not cover the new Debug tree.

Keep the ordinary small fixture (`*-scene.txt`) and the full package (`*-scene-package.txt`) separate. Each configuration records `*-scene-package.json` with actual package byte count/SHA256 before execution, verifies unchanged identity afterward, and retains `*-scene-package.bmp`: a **1280×720 GPU post-processed sRGB capture**, not a desktop/scanout screenshot. The fresh-output guard also rejects existing files/links and concurrent file creation rather than overwriting owner data. Missing image output, changed input or failed native execution stops verification.

The full-package output must retain all geometry/index/mip readbacks and cancellation/frame-pin retirement markers, plus:

- `resident_vertices=1304323 resident_indices=3309060 resident_images=4 retained_lights=612` and `resident_verified_mip_bytes=89478480`;
- `scene_lod=2 scene_triangles=238080 scene_draws=188 scene_primitives=188 scene_materials=13 scene_light_instances=612`;
- `scene_hdr_width=1280 scene_hdr_height=720`, `scene_capture=1`, `generated_ibl=1 generated_shadow=1 shadow_extent=1024` and `scene_wsi_queued=1`;
- native resource controls `generated_shadow=1 shadow_indexed_transform=1 shadow_mask_uv1_sampler=1 shadow_blend_excluded=1 shadow_receiver_visibility=1` and prior IBL/resource/WSI markers.

The logs also record actual WSI extent; window-system scaling need not equal offscreen capture dimensions. Lit-pixel counts may differ across drivers; require finite nonblack output, not Intel byte/pixel-count identity. Preserve the shader manifest and **seven** reflections, actual CTest listing/results, compiler/profile identity and clean ordinary validation output. Callback calibration remains separately intentional. The visible test windows exit automatically; report unexpected behavior rather than changing setup or bypassing policy.

Send the two newly printed evidence ZIPs, identified as MSVC/ClangCl, or the failure ZIP if execution stops. Review the captures for missing/misplaced geometry, inverted normals, texture/color/UV errors and shadow artifacts; report observations separately from automated success. This orthographic ship-only fixture does not validate asteroid instancing, the inspector, bloom, warmed performance, whole-memory budgets or [final acceptance](../acceptance.md).
