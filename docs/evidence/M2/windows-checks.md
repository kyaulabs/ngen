# Windows verification handoff

CPU compiler checks passed: [MSVC 19.44.35229 Debug/Release](../M1/windows-msvc/README.md) at `d49b23c` and [clang-cl 19.1.5 Debug/Release](../M1/windows-clang-cl/README.md) at `54a7a63`. Each configuration passed four tests and both replays with zero measured steady-state allocations. Formal milestone acceptance and Windows graphics checks remain open.

The [Windows GPU capture](windows-nvidia/README.md) identifies an RTX 5070 with driver 617.42 and the required queried device features. It does not enumerate `VK_LAYER_KHRONOS_validation`, so Windows graphics validation is not ready. Install/enable the Windows Vulkan SDK's validation layers, then open a new PowerShell window and repeat the capture:

```powershell
git pull --ff-only
pwsh -NoProfile -File .\tools\build-windows.ps1 -GpuInfo
```

This mode runs only `vulkaninfo --summary` and `vulkaninfo --json`, recording the executable hash/version and actual GPU/device/driver/features. It does not rebuild the CPU targets, load the MSVC environment or prepare Python tools. The script searches PATH and `$env:VULKAN_SDK\Bin\vulkaninfo.exe`. If missing, install the Windows Vulkan SDK with Vulkan Tools from <https://vulkan.lunarg.com/sdk/home#windows>, open a new PowerShell window and rerun the same command. The script does not install SDKs, drivers or system packages, require administrator access, or alter execution policy.

The script prints a new `build/windows/<timestamp>/evidence.zip` path on success or failure. Send that archive, not an earlier run's ZIP. It includes source revision, working-tree state, tool identity, summary, generated GPU JSON and diagnostics. Missing JSON, invalid profile data and failed native commands are errors. GPU capture is not renderer validation, a supported-tier decision, or proof of presentation.

For later CPU regressions, the same script supports:

```powershell
pwsh -NoProfile -File .\tools\build-windows.ps1
pwsh -NoProfile -File .\tools\build-windows.ps1 -Compiler ClangCl
```

Both CPU modes test Debug and Release by default; append `-Configuration Debug` to select one configuration. Clang-cl is found through PATH, Visual Studio's LLVM x64 directory or the standard LLVM installation. If absent, add C++ Clang tools for Windows through Visual Studio Installer. Profiles and builds live in fresh generated directories; existing user profiles and earlier evidence are preserved. The Clang host profile uses the actual version and active MSVC runtime family; the build profile uses detected MSVC. Conan and CMake both select Ninja. Graphics remain disabled during CPU checks.

Linux-hosted PowerShell tests cover native exit propagation, output/empty-log capture, argument paths, actual Conan profile resolution, CPU/GPU mode exclusion, missing/unrelated GPU JSON and directory restoration. The GPU capture function was also exercised with the actual local Intel device through vulkaninfo; that is not Windows execution evidence. No new test framework was added. The earlier warning-policy failure and its correction remain in [the first clang-cl attempt](../M1/clang-cl-attempt/README.md).

Later M2 acceptance still requires actual Windows SDL/Vulkan startup, swapchain/resize/lifetime validation, shader output, rendered-scene checks and performance evidence. Those implementations and captures must not be inferred from these CPU or capability-query results. Check logs before sharing and do not include credentials. The script never reads credential files. If execution policy blocks it, report the error rather than bypassing policy.
