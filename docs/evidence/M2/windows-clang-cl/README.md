# Windows clang-cl Vulkan startup verification

Owner-executed on 2026-11-05 at `c730a18a541f66a294f620e88e680985f0160be3`, with a clean checkout, using `tools/build-windows.ps1 -Renderer -Compiler ClangCl`. **Debug and Release builds, all five tests, callback calibration and clean validation-enabled startup/teardown passed.** Together with the [MSVC results](../windows-msvc/README.md), this completes the Windows compiler checks for the current startup slice. It does not establish frame rendering, presentation or M2 acceptance.

| Item | Observed value |
|---|---|
| Host compiler / target | clang-cl 19.1.5 / `x86_64-pc-windows-msvc` |
| Conan host / build profiles | Clang 19 / MSVC 194, both x86_64 and C++20 |
| Host runtime | Dynamic v144, matching Debug/Release types |
| SDL | Locked Conan SDL 3.4.16, shared; built from source in both configurations |
| SDK setting | `C:\VulkanSDK\1.4.363.1` |
| Vulkan discovery | SDK `Lib/vulkan-1.lib`, header version 1.4.363 |
| Device / SDL driver | NVIDIA GeForce RTX 5070 / `windows` |
| Device API in startup log | 1.4; patch version is not printed |
| Raw NVIDIA driver version | 2588573696, decoding to major/minor 617.42 |
| Selected queue family | 0 |

| Check | Debug | Release |
|---|---|---|
| Owned targets compile/link | Passed, 46/46 build steps | Passed, 46/46 build steps |
| CTest | 5/5 passed | 5/5 passed |
| Normal validation-enabled startup | Passed, no validation messages | Passed, no validation messages |
| Callback calibration | Exactly one labelled intentional error | Exactly one labelled intentional error |
| Serial/two-worker replays | Sum 11930; zero measured allocations | Sum 11930; zero measured allocations |

The source at this revision creates the SDL window and Vulkan surface, checks the actual surface's graphics/presentation queue support, enables the required features, and verifies shutdown and cleared borrowed handles. No swapchain is created and no frame is submitted or presented. The hidden window exits automatically; no screenshot or Windows sanitizer result is claimed.

Each calibration invocation deliberately submits `expected NGEN validation callback self-test`. That diagnostic appears exactly once in its separate calibration log. Both normal startup logs contain only the device/queue/display line. Calibration is not represented as a clean validation pass.

`Debug-dependencies.txt` and `Release-dependencies.txt` retain effective profiles and selected package graphs before installation output. Account-bearing compiler-executable configuration lines were omitted. The Clang host is not relabelled as MSVC; the separate build profile uses MSVC. Cached dependencies are not represented as newly compiled packages. All four explicit CPU replays use seed `0x4e47454e`, 8 entities, 1,000 ticks and 32,000 commands, with `allocation_probe=1`, `new_allocations=0` and `scheduler_allocations=0`. Timings are exploratory, not graphics performance evidence.

Original archive SHA-256: `6c549d5c7b29439917a542229dbd36b2bb82bc572c167871aca8508014910721`. The archive was listed and passed a redacted Gitleaks scan before inspection. Unlike the previous duplicate upload, it contains actual clang-cl renderer results. Selected logs use normalized LF/trailing whitespace; `files.sha256` identifies their bytes. Full installation output and the session transcript are omitted. This evidence adds no engine changes.

No further Windows setup is required for this startup slice. The [Linux SDL repair](../wayland-repair/README.md) now passes the previously failing Wayland workloads while preserving Windows' recipe. Additional surface/lifetime behavior, independent acceptance and M2-B through M2-F remain open. Later rendering work will need new Windows execution evidence; these startup runs cannot supply it.
