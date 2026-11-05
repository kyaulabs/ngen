# Windows clear-only presentation verification

The owner ran the Windows helper from MSYS2 fish at clean source revision `b7f0334f3a932d3717c0a927d126f48c96f6f9cb`. Both compiler archives were listed, hashed and scanned with redacted Gitleaks archive support before inspection. Scans covered 50,374 and 53,291 uncompressed bytes and found no leaks. The original archives remain outside the repository; selected logs below are normalized to LF with trailing whitespace removed. Full account-bearing transcripts, tool-install output and profile-detection chatter are not committed. `files.sha256` fingerprints the retained files.

| Archive | Compiler | SHA-256 |
|---|---|---|
| `evidence1.zip` | MSVC | `7946008b4fae3310452ac684b6eb8f94d023e5997f5cf620b3f277370d4ffe67` |
| `evidence2.zip` | clang-cl | `65c9ce9c6744f6b547ad3a458ab8689aa2ef07043a5e0f218b96f2f36d37c5aa` |

All four configurations built and linked all owned targets in 52/52 steps without build warnings/errors, enumerated and passed 5/5 CTest entries without skips, and completed both explicit CPU replays. Replays preserve sum 11930 and `allocation_probe=1 new_allocations=0 scheduler_allocations=0`. Hidden-window startup has no validation diagnostics. Callback calibration has exactly one labelled intentional error, separately from the presentation scenario.

Every `*-presentation.txt` reports:

```text
device=NVIDIA GeForce RTX 5070 api=1.4 driver=2588573696 queue=0 display=windows
surface=1280x720 format=50 color_space=0 present_mode=2 images=3
queued_frames=96 swapchain_generations=3 verified_resizes=2 minimize_verified=1
```

This verifies actual Win32 acquisition/clear/presentation requests, three completed swapchain generations, two observed resizes and minimize/skip/restore in the public probe. The presentation profile explicitly enables synchronization validation; all four presentation logs are free of validation diagnostics. Successful probe exit also checks shutdown, repeated close, closed-surface/frame rejection and cleared borrowed handles. These are accepted presentation requests, not proof of scanout, captured pixel correctness or a rendered PBR scene.

MSVC is 19.44.35229.0; clang-cl is 19.1.5 targeting `x86_64-pc-windows-msvc`, with a Clang 19 host and MSVC 194 build profile. All effective Conan input graphs and CMake configurations use x64/C++20 and matching dynamic Debug/Release CRTs; clang-cl uses runtime family v144. The raw detected MSVC profile still says C++14; command-line overrides in `*-conan.txt` and effective CMake output establish C++20. Do not interpret that raw profile or compatible cached dependencies as fresh C++20 dependency compilation. All dependencies were cached, including the unchanged upstream shared SDL 3.4.16 recipe `fe421775c8e1866eb22a3ba23ef8fd62`; the Linux-only patched recipe is not selected. SDK discovery found `C:\VulkanSDK\1.4.363.1`, Vulkan 1.4.363; the existing RTX 5070 driver identity is unchanged. This run installs no SDK or driver and needs no persistent environment/registry changes.

This closes the requested Windows first-light execution gap for both compilers and configurations. No new setup or rerun is needed for this slice. Windows sanitizer execution, general GPU resource replacement/unload, surface/device-loss recovery, pixel captures, shaders/assets/PBR, performance and independent milestone acceptance are not established. M2 remains incomplete. The [Linux first-light evidence](../presentation-first-light/README.md) records its separate instrumented and injected-failure workloads; [earlier Windows startup evidence](../windows-msvc/README.md) remains historical, not overwritten by this run.

Same-session evidence review checked archive/source identity, clean working trees, compiler/CRT/standard and dependency selection, all build/test counts, normal versus intentional validation output, replay allocation results and all frame/resize/minimize counters. It is not independent graphics acceptance or owner approval of the milestone. The next implementation slice is bounded GPU resource ownership and submission-based retirement, not another Windows setup exercise.
