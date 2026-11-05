# Windows MSVC Vulkan startup verification

Owner-executed on 2026-11-05 at `c730a18a541f66a294f620e88e680985f0160be3`, with a clean checkout, using `tools/build-windows.ps1 -Renderer`. **Debug and Release configure, compilation, linking, tests and validation-enabled startup/teardown passed.** This verifies the Windows MSVC startup slice, not frame rendering or M2 acceptance.

| Item | Observed value |
|---|---|
| Compiler | MSVC 19.44.35229.0, x86_64 |
| NGEN language mode | C++20 |
| Runtime | Dynamic, matching Debug/Release types |
| SDL | Conan SDL 3.4.16, shared |
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

The source at this revision creates the SDL window and Vulkan surface, queries presentation support on that surface, selects a graphics/presentation queue, enables the required features, and checks shutdown results and cleared borrowed handles. The successful probes therefore establish actual Win32 surface creation and queue support, beyond the earlier capability-only capture. They create no swapchain and submit or present no frame.

Each calibration invocation deliberately submits `expected NGEN validation callback self-test`. Its one diagnostic belongs only to the calibration log; it is not ignored during normal startup. Both separate normal startup logs contain only the device/queue/display line. No Windows sanitizer or performance acceptance result is claimed.

The SDK inheritance correction is confirmed: `vulkan-sdk.txt` records the configured SDK and both CMake logs resolve its import library. The [original discovery failure](../windows-render-attempt/README.md) remains preserved. No reinstall, persistent environment edit or driver change was needed.

`Debug-dependencies.txt` and `Release-dependencies.txt` retain the effective host/build profiles and selected graph before package installation output. Both contexts use C++20 and matching dynamic runtime types; the preliminary detected profile's C++14 default is overridden by the script. Release reuses a Conan-compatible C++14 enkiTS package, as recorded in the graph; this is not relabelled as a fresh C++20 dependency build. SDL Debug was cached and Release was built from source. Full Conan logs containing account paths and the session transcript were omitted.

The replays retain the original fixture: seed `0x4e47454e`, 8 entities, 1,000 ticks and 32,000 commands. `allocation_probe=1`, `new_allocations=0` and `scheduler_allocations=0` in all four explicit invocations. Their exploratory timings are not graphics or fleet-capacity evidence.

Original archive SHA-256: `8c7a9e83f8290e3037d8687ba47ecc1877fc0227c9ae0d184e61502337bbec8b`. The ZIP passed a redacted Gitleaks archive scan before inspection. Selected outputs retain normalized LF/trailing whitespace; `files.sha256` identifies their bytes. The owning evidence commit records only documentation and logs, not engine changes.

Follow-up: the [clang-cl renderer check](../windows-clang-cl/README.md) also passed Debug/Release at this revision. No further Windows setup is currently needed; the [handoff](../windows-checks.md) retains regression commands. M2-A remains partial: the reproduced Wayland LeakSanitizer failure, zero-size/surface/swapchain handling and independent acceptance remain open. M2-B through M2-F, shaders, cooked assets, PBR passes, inspector and graphics performance evidence are not supplied by this run.
