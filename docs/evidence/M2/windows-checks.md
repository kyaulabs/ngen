# Windows verification handoff

CPU compiler checks passed: [MSVC 19.44.35229 Debug/Release](../M1/windows-msvc/README.md) at `d49b23c` and [clang-cl 19.1.5 Debug/Release](../M1/windows-clang-cl/README.md) at `54a7a63`. Each configuration passed four tests and both replays with zero measured steady-state allocations. Formal milestone acceptance and Windows graphics checks remain open.

The [Windows GPU capture](windows-nvidia/README.md) identifies an RTX 5070 with driver 617.42 and the required queried device features. The [follow-up capture](windows-validation/README.md) now enumerates `VK_LAYER_KHRONOS_validation` 1.4.363 without the earlier loader warning. Layer availability is confirmed; application validation is not.

Run the next check in a new PowerShell window in this checkout:

```powershell
git pull --ff-only
pwsh -NoProfile -File .\tools\build-windows.ps1 -Renderer
```

This builds Debug and Release with MSVC, using the separate `conan-render.lock` and project-managed SDL 3.4.16. It needs the core x64 Vulkan SDK headers/library and visible validation layers, not the SDK's optional SDL bundle. Conan's runtime environment supplies SDL's DLL path per configuration and is restored afterward. Append `-Compiler ClangCl` for the second compiler, or `-Configuration Debug` to test one configuration first.

The [first renderer attempt](windows-render-attempt/README.md) stopped at CMake discovery because the terminal lacked `VULKAN_SDK`, although the machine setting and `vulkan-1.lib` existed. The script now reads process → user → machine SDK settings, preserving an explicit process override and setting only its own process environment. `vulkan-sdk.txt` records the selected path. No terminal restart or persistent environment edit is required for this case; the Windows rerun remains pending.

Each configuration runs five CTest entries, the two CPU replays, callback calibration and normal validation-enabled SDL/Vulkan startup/teardown. Calibration deliberately prints `expected NGEN validation callback self-test`; that specific diagnostic is expected only in `*-validation-callback.txt`. The separate `*-render-startup.txt` must have no validation errors. Startup checks the actual SDL surface's queue presentation support, but submits no frame and tests no swapchain presentation or drawing. The probe uses a hidden window and exits automatically; no screenshot is expected.

The script prints a new `build/windows/<timestamp>/evidence.zip` path on success or failure. Send that archive, not an earlier run's ZIP. No SDK, driver or system package is installed by the script; it does not require administrator access or alter execution policy. Failed native commands stop the run and retain diagnostics.

For a later capability-only capture, use `pwsh -NoProfile -File .\tools\build-windows.ps1 -GpuInfo`. That separate mode runs only `vulkaninfo --summary` and `--json`, records tool identity and generated profiles, and does not load MSVC or prepare Python. It searches PATH and `$env:VULKAN_SDK\Bin\vulkaninfo.exe`. Missing/unrelated JSON and native failures are errors; it cannot be combined with renderer/CPU parameters.

For later CPU regressions, the same script supports:

```powershell
pwsh -NoProfile -File .\tools\build-windows.ps1
pwsh -NoProfile -File .\tools\build-windows.ps1 -Compiler ClangCl
```

Both CPU modes test Debug and Release by default; append `-Configuration Debug` to select one configuration. Clang-cl is found through PATH, Visual Studio's LLVM x64 directory or the standard LLVM installation. If absent, add C++ Clang tools for Windows through Visual Studio Installer. Profiles and builds live in fresh generated directories; existing user profiles and earlier evidence are preserved. The Clang host profile uses the actual version and active MSVC runtime family; the build profile uses detected MSVC. Conan and CMake both select Ninja. Graphics remain disabled during CPU checks.

Linux-hosted PowerShell tests cover native exit propagation, output/empty-log capture, argument paths, actual Conan profile resolution, mode exclusion, missing/unrelated GPU JSON, renderer runtime restoration/short-circuiting on failures, and SDK setting precedence/inheritance by real child processes. The GPU capture function was also exercised with the actual local Intel device through vulkaninfo; that is not Windows execution evidence. No new test framework was added. The earlier warning-policy failure and its correction remain in [the first clang-cl attempt](../M1/clang-cl-attempt/README.md).

The [locked Linux startup checks](renderer-startup/README.md) passed Debug/Release startup and all three configurations' headless tests, but actual Wayland GPU execution under LeakSanitizer reports a reproducible 480-byte dependency leak. That remains open; it does not require a Windows driver change. Later M2 acceptance still requires resolution of that leak, actual Windows SDL/Vulkan startup, swapchain/resize/lifetime validation, shader output, rendered-scene checks and performance evidence. Those implementations and captures must not be inferred from these CPU or capability-query results. Check logs before sharing and do not include credentials. The script never reads credential files. If execution policy blocks it, report the error rather than bypassing policy.
