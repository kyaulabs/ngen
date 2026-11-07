# Windows verification handoff

Latest combined [MSVC/clang-cl Debug and Release execution](windows-frigate-verified/README.md) at `ef984ef` passes 8/8 tests each and native RTX 5070 full-frigate checks, including generated IBL/shadows, complete readbacks and byte-identical 1280×720 post-processed captures. The full-package Windows execution slice is complete; no duplicate run or SDK/driver setup is requested. [Earlier Debug fixture evidence](windows-scene-debug/README.md) at `e387dde` remains historical and does not itself validate later passes. Commands below and the [full-package workflow](windows-frigate/README.md) are retained for reproduction, not final M2 acceptance.

CPU compiler checks passed: [MSVC 19.44.35229 Debug/Release](../M1/windows-msvc/README.md) at `d49b23c` and [clang-cl 19.1.5 Debug/Release](../M1/windows-clang-cl/README.md) at `54a7a63`. Each configuration passed four tests and both replays with zero measured steady-state allocations. Formal milestone acceptance and Windows frame-rendering checks remain open.

The [Windows GPU capture](windows-nvidia/README.md) identifies an RTX 5070 with driver 617.42 and the required queried device features. The [follow-up capture](windows-validation/README.md) now enumerates `VK_LAYER_KHRONOS_validation` 1.4.363 without the earlier loader warning. The [MSVC renderer run](windows-msvc/README.md) and [clang-cl renderer run](windows-clang-cl/README.md) at `c730a18` both pass Debug/Release builds, 5/5 tests each, callback calibration and clean validation-enabled Win32 startup/teardown. That startup revision did not test frame rendering or swapchain presentation.

**Both compiler modes pass [Windows clear/resize/minimize presentation](windows-first-light/README.md) at `b7f0334`; that first-light check needs no further setup.** The [GPU resource slice](gpu-resources/README.md) now also passes [MSVC and clang-cl Debug/Release execution](windows-resources/README.md) at `dbdd3ba`: 66/66 build steps, 5/5 tests, exact transfers/retirement, stable replacement plateau and clear/resize/minimize checks. No further setup or rerun is needed for that resource slice. The combined [shader/reflection](shader-interface/README.md), [production indexed PBR](production-mesh/README.md) and [checked scene/frame-pin](checked-scene-draw/README.md) path now passes owner Debug verification in both compilers; Release and later renderer revisions still need owner execution. `-Scene` includes the renderer, offline shaders and importer-free checked runtime; its built-in small scene needs no private frigate source/package on Windows. No new SDK/driver setup is requested. From MSYS2 fish:

```fish
cd /d/Git/kyaulabs/ngen
and git fetch --prune origin
and git switch --detach ef984ef18c6fb6ed9886f45003dc67e85d42170c
and begin
    for compiler in MSVC ClangCl
        pwsh.exe -NoProfile -File (cygpath -w "$PWD/tools/build-windows.ps1") -Scene -Compiler $compiler
        or break
    end
end
```

Or, from PowerShell 7:

```powershell
foreach ($compiler in 'MSVC', 'ClangCl') {
    pwsh -NoProfile -File .\tools\build-windows.ps1 -Scene -Compiler $compiler
    if ($LASTEXITCODE -ne 0) { break }
}
```

Send both newly printed combined-scene evidence archives (identify MSVC versus ClangCl), or the failure archive if a run stops. Earlier resource ZIPs do not verify this new code. Each compiler runs Debug and Release. The visible test window resizes, minimizes, restores and exits automatically; do not close it manually. No new SDK/driver setup is requested.

ClangCl uses the detected clang-cl host compiler and MSVC build profile; MSVC uses its detected native profile. `-Scene` uses `conan-scene.lock`, including the already pinned stock Windows SDL 3.4.16 and Zlib runtime dependency; Linux retains its separate namespaced SDL repair. Offline importers/cookers/KTX tools are not added to this runtime graph. It needs the core x64 Vulkan SDK headers/library and visible validation layers, not the SDK's optional SDL bundle. Conan's runtime environment supplies SDL's DLL path per configuration and is restored afterward. Append `-Configuration Debug` to test one configuration first. Use `-Compiler MSVC` or `-Compiler ClangCl` to select one compiler when investigating a failure.

The [first renderer attempt](windows-render-attempt/README.md) stopped at CMake discovery because the terminal lacked `VULKAN_SDK`, although the machine setting and `vulkan-1.lib` existed. The script now reads process → user → machine SDK settings, preserving an explicit process override and setting only its own process environment. `vulkan-sdk.txt` records the selected path. The MSVC rerun confirmed SDK discovery at `C:\VulkanSDK\1.4.363.1`, without a terminal restart or persistent environment edit.

Each configuration lists and executes its actual CTest entries, both CPU replays, separate callback calibration/startup/presentation, `resource_probe` (`*-resources.txt`), `shader_gpu_probe` (`*-shaders.txt`) and `scene_probe` (`*-scene.txt`). Do not assume a Linux test count on Windows. Shader output requires six cull counts/1,152 exact values, `pbr_draws=16`, `two_sided_normal=1`, UV1/per-map sampler/absent-normal and previous matrix/material markers, plus `shader_gpu=1 validation_clean=1`. The current manifest and all **seven** reflection files (including shadow vertex/fragment) retain actual tool/library/interface identities. Slang/SPIR-V tools remain build-profile Release dependencies, independent of engine configuration, not runtime libraries.

Resource output must retain prior exact transfers, 128 replacements, resize/minimize/retirement checks and now `tracked_indexed_pbr=1 mesh_depth=1 mesh_materials=2 mesh_retirement=1 mesh_two_slots=1`, HDR post/AA and sampled-image WSI controls. The current scene fixture must report `checked_scene_pbr=1 generated_ibl=1 generated_shadow=1 shadow_extent=1024`, two frame versions, retired frame pins, queued WSI, finite nonblack HDR pixels, complete checked geometry/mip readbacks and cancellation/close markers. Plain `-Scene` is **not the full frigate on NVIDIA**, performance or visual acceptance. Supply `-ScenePackage` for the full private input; its separate log and capture must not be mistaken for the built-in fixture. Earlier `e387dde` results did not exercise these later environment/shadow additions. Queued WSI is not displayed-pixel proof.

Calibration's intentional `expected NGEN validation callback self-test` belongs only in `*-validation-callback.txt`; startup, presentation, resources, shaders and scene logs must have no unexpected validation/sanitizer findings. Presentation still requires 96 queued frames, at least three generations, two verified resizes and Windows `minimize_verified=1`. Report unexpected window behavior; no manual screenshot, driver, SDK or persistent setup change is requested. `-ScenePackage` saves its offscreen post-processed capture automatically.

The script prints a new `build/windows/<timestamp>/evidence.zip` path on success or failure. Send that archive, not an earlier run's ZIP. No SDK, driver or system package is installed by the script; it does not require administrator access or alter execution policy. Failed native commands stop the run and retain diagnostics.

For a later capability-only capture, use `pwsh -NoProfile -File .\tools\build-windows.ps1 -GpuInfo`. That separate mode runs only `vulkaninfo --summary` and `--json`, records tool identity and generated profiles, and does not load MSVC or prepare Python. It searches PATH and `$env:VULKAN_SDK\Bin\vulkaninfo.exe`. Missing/unrelated JSON and native failures are errors; it cannot be combined with renderer/CPU parameters.

For later CPU regressions, the same script supports:

```powershell
pwsh -NoProfile -File .\tools\build-windows.ps1
pwsh -NoProfile -File .\tools\build-windows.ps1 -Compiler ClangCl
```

Both CPU modes test Debug and Release by default; append `-Configuration Debug` to select one configuration. Clang-cl is found through PATH, Visual Studio's LLVM x64 directory or the standard LLVM installation. If absent, add C++ Clang tools for Windows through Visual Studio Installer. Profiles and builds live in fresh generated directories; existing user profiles and earlier evidence are preserved. The Clang host profile uses the actual version and active MSVC runtime family; the build profile uses detected MSVC. Conan and CMake both select Ninja. Graphics remain disabled during CPU checks.

Linux-hosted PowerShell tests cover native exit propagation, output/empty-log capture, argument paths, actual Conan profile resolution, mode exclusion, missing/unrelated GPU JSON, renderer/scene execution and runtime restoration/short-circuiting on failures, and SDK setting precedence/inheritance by real child processes. The GPU capture function was also exercised with the actual local Intel device through vulkaninfo; that is not Windows execution evidence. No new test framework was added. The earlier warning-policy failure and its correction remain in [the first clang-cl attempt](../M1/clang-cl-attempt/README.md).

The [locked Linux startup checks](renderer-startup/README.md) originally failed LeakSanitizer with a dependency leak. The [investigation](wayland-tablet-leak/README.md) traced surviving tablet/pad proxies, and the owner-approved [Linux-only SDL repair](wayland-repair/README.md) now passes the instrumented SDL-only and real GPU startup workloads. Both Windows graphs retain the verified stock SDL recipe; no Windows driver, setup change or rerun is needed for this repair. Linux clear-only acquisition, resize and presentation-retirement checks pass separately; the owner's new Windows runs now verify the public first-light scenario in both compilers/configurations. Later M2 acceptance also requires broader lifetime validation, shader output, rendered-scene checks and performance evidence. Those implementations and captures must not be inferred from these CPU or capability-query results. Check logs before sharing and do not include credentials. The script never reads credential files. If execution policy blocks it, report the error rather than bypassing policy.
