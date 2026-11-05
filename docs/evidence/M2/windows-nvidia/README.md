# Windows NVIDIA capability capture

Owner-executed on 2026-11-04 at `dc8eb76f0b9e800098e0d1041a26fd4d0f7a9bd3`, using `tools/build-windows.ps1 -GpuInfo`. The query completed and generated a Vulkan Profiles JSON file. This is capability evidence, not a rendering, presentation or validation pass.

| Item | Observed value |
|---|---|
| GPU | NVIDIA GeForce RTX 5070, discrete, vendor 0x10de/device 0x2f04 |
| Driver | NVIDIA proprietary 617.42; reported driverVersion 617.42.0.0 |
| Device API | Vulkan 1.4.351 |
| Instance API | Vulkan 1.4.341 |
| Tool | `C:\Windows\System32\vulkaninfo.exe` |
| Tool SHA-256 | `da2aac11acf19302362d6dc0223524e69992d39a18de96c63e5e0c3dc278a735` |

The profile reports dynamic rendering, synchronization2 and timeline semaphores; a graphics queue; `VK_KHR_swapchain`; B8G8R8A8_SRGB optimal color-attachment support; and D32_SFLOAT optimal depth-attachment support. `capability-check.txt` records these checks against the JSON fields, matching the queried portions of the current CPU capability policy. The instance exposes `VK_KHR_win32_surface`. Neither that extension nor a graphics queue proves that the eventual SDL surface has presentation support; startup must query it on the actual surface.

**Windows validation remains blocked.** `summary.txt` lists four NVIDIA/Steam layers but no `VK_LAYER_KHRONOS_validation`. The loader also reports a registry layer-manifest lookup warning. The exact cause is not established; the capture does not prove that the Vulkan SDK is absent, and no registry or driver change was attempted. The current validation-enabled renderer explicitly rejects a missing Khronos layer.

Before the owner can run Windows graphics validation, install/enable the Vulkan SDK's validation-layer component from <https://vulkan.lunarg.com/sdk/home#windows>, open a new PowerShell window, and repeat `tools/build-windows.ps1 -GpuInfo`. If the SDK is already installed, confirm its validation layers are installed and visible to the loader rather than reinstalling the GPU driver. The next summary must enumerate `VK_LAYER_KHRONOS_validation`. The recorded GPU capabilities give no reason to request a driver upgrade.

The SDK/compiler identity is not inferred from the system32 tool or the driver API version. No swapchain, shader execution, presentation, performance or GPU resource lifetime result is claimed. M2-A acceptance remains open; M2-B through M2-F remain unimplemented.

Original archive SHA-256: `95ef75abcf2b004be714f105384c1c1cf2bfe7aac8ce3b874d9ee00bd0ee82d3`. The original `VP_VULKANINFO_NVIDIA_GeForce_RTX_5070_617_42_0_0.json` is retained as `profile.json`. Other selected command outputs are retained alongside it; line endings and trailing whitespace are normalized. `files.sha256` identifies those retained bytes. The archive passed a redacted Gitleaks scan before inspection. The account-bearing transcript was not committed.
