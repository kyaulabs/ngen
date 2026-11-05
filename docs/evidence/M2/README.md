# M2 startup evidence

Status: **M2-A partial; M2 is not complete.** M2-B through M2-F are not implemented. The owner's Windows [MSVC](../M1/windows-msvc/README.md) and [clang-cl](../M1/windows-clang-cl/README.md) CPU Debug/Release runs passed at `d49b23c` and `54a7a63`. The [Windows RTX 5070 capability capture](windows-nvidia/README.md) succeeded, and the [follow-up](windows-validation/README.md) confirms Khronos validation-layer availability. [Windows MSVC Debug/Release startup](windows-msvc/README.md) passed at `c730a18`, including 5/5 tests each and clean validation-enabled Win32 startup/teardown. Clang-cl renderer execution, frame rendering/presentation, a local Wayland sanitizer leak and formal acceptance remain open. Do not equate this Vulkan startup slice with a renderer, a cooked asset pipeline or the PBR milestone.

The user authorized continuing through M2, supplied `planning/models/frigate/frigate-01.glb`, authorized SSH probing of `apollo.kyaulabs`, and retained Windows/NVIDIA testing as a manual owner task. Existing planning documents and model files remain in place and ignored as before.

## Implementation and local verification

`ngen_render_contract` tests the Vulkan 1.3 capability policy without linking graphics into `ngen_core`, `ngen_jobs`, `ngen_simulation` or `sim_harness`. `NGEN_BUILD_RENDERER=ON` adds `ngen_render` and `render_probe`, with SDL window creation, Vulkan surface/device/queue selection, explicit feature enablement and RAII cleanup. Startup uses bounded device, queue, extension and layer counts. Unsupported devices and missing validation layers fail explicitly.

The initial slice below used installed SDL 3.4.18 and Vulkan loader/headers 1.4.363 via CMake discovery. The [locked startup follow-up](renderer-startup/README.md) now builds SDL 3.4.16 through optional `renderer=True` and `conan-render.lock`, separately from the unchanged CPU graph. Debug/Release builds, five tests per configuration, and Intel/Wayland startup passed; ASan/UBSan headless tests also passed. Actual Wayland GPU startup under LeakSanitizer reports a 480-byte leak, reproduced by SDL init/quit alone and the prior system-SDL build. It remains an acceptance blocker. No full graphics stack or completed renderer is claimed.

```sh
cmake --preset debug -B build/render-system -DCMAKE_TOOLCHAIN_FILE="$PWD/build/debug/conan_toolchain.cmake" -DNGEN_BUILD_RENDERER=ON
cmake --build build/render-system -j2
ctest --test-dir build/render-system --output-on-failure --no-tests=error

# Compatibility probe without validation; not a validation pass:
build/render-system/render_probe --without-validation

# Validation-enabled startup using the isolated local layer package:
VK_LAYER_PATH="$PWD/build/validation-layer/usr/share/vulkan/explicit_layer.d" LD_LIBRARY_PATH="$PWD/build/validation-layer/usr/lib" build/render-system/render_probe
```

Clang 23.1.4 Debug: 5/5 CTest entries passed, including the baseline capability policy and explicit unavailable-video-driver cleanup. A separate ASan/UBSan build also passed 5/5:

```sh
cmake --preset asan -B build/render-asan -DCMAKE_TOOLCHAIN_FILE="$PWD/build/asan/conan_toolchain.cmake" -DNGEN_BUILD_RENDERER=ON -DCMAKE_EXE_LINKER_FLAGS=-no-pie
cmake --build build/render-asan -j2
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir build/render-asan --output-on-failure --no-tests=error
```

SDL/Vulkan system binaries are not rebuilt with sanitizers; owned code is instrumented. The GPU startup probe passed on Iris Xe using the Wayland driver, with no validation messages, and verifies repeated close plus cleared borrowed handles. A missing validation layer originally failed with a useful message. The isolated Arch validation-layer package was downloaded into ignored `build/validation-layer`, not installed system-wide: version 1.4.363.0-1, SHA-256 `904f4eba2e708c9c96579900688a284309cd56dc09b5af3f78c563ffda13df9e`, from `https://losangeles.mirror.pkgbuild.com/extra/os/x86_64/vulkan-validation-layers-1.4.363.0-1-x86_64.pkg.tar.zst`. Its checksum was verified against package-manager metadata before extraction.

`startup-tests.txt`, `startup-asan-tests.txt`, `startup-smoke.txt` and `startup-tidy.txt` retain local results. Formatting and clang-tidy covered all five new translation units and their consumers using `build/render-system/compile_commands.json`. The baseline regression first failed when Vulkan 1.2 was incorrectly accepted, then passed with individual required-feature rejection tests.

## Hardware and asset facts

- `iris-xe-profile.json` and `iris-xe-summary.txt`: actual local Iris Xe / Mesa 26.2.6, device API 1.4.354, Wayland. Dynamic rendering, synchronization2, timeline semaphores and swapchain maintenance are reported supported. Actual startup enabled only the features currently used. No rendering/performance claim follows from these queries.
- `nvidia-profile.json` and `nvidia-summary.txt`: actual GTX 1660 SUPER / NVIDIA 615.78.08 on authorized `apollo.kyaulabs`, device API 1.4.351. The host lacked vulkaninfo and the generic loader; copies were placed only in `/tmp/ngen-m2-rnMZnG` and run with a process-local library path. No system packages, services or infrastructure were changed. `nvidia-probe-stderr.txt` records missing DISPLAY: this is device capability evidence, **not NVIDIA surface/presentation evidence**.
- [Windows RTX 5070 profile](windows-nvidia/README.md): owner-run Vulkan query at `dc8eb76`, NVIDIA 617.42 and device API 1.4.351. Required device features/formats are reported supported. The [follow-up](windows-validation/README.md) reports instance API 1.4.363 and visible Khronos validation 1.4.363 without the earlier loader warning. The [MSVC renderer run](windows-msvc/README.md) now verifies Win32 surface/queue support and clean validation-enabled startup/teardown in Debug and Release. No frame rendering or swapchain presentation was tested.
- The supplied frigate's SHA-256 is `e68cd409c479036556446ed1753b287c0e7a40260109b3cd442abe7f533941fe`; GLB v2, 137,812,624 bytes, 41 meshes, 654 nodes, 13 materials and 4 embedded PNG images. Its required extension is `KHR_lights_punctual`. The adjacent owner-provided manifest reports 574,136 triangles and 612 lights. These are inputs, not engine validation results. The future cooker must explicitly support or reject that extension; it must not silently drop it. Texture/geometry/light reduction policies must be measured against the laptop fixture budget, not silently change the fixture. No model, texture or Blender source was modified or committed. Distribution-rights approval is not inferred beyond the supplied development input.

## Review and remaining gates

Independent read-only agent reviews are retained in `startup-review.txt` and `startup-rereview.txt`. They are not human approval. Fixed findings: SDL error retrieval ordering, loss of device rejection reasons, sticky/reportable shutdown failure, stale borrowed handles after close, and family-specific rejection diagnostics that previously overwrote one another. The last fix labels and retains each rejected queue's reason. A simulated `vkDeviceWaitIdle` failure is covered by the [startup follow-up](renderer-startup/README.md). Actual driver loss and in-flight retirement remain untested.

Remaining: the graphics dependency graph beyond SDL and Windows clang-cl renderer execution; resolution of the retained Wayland sanitizer leak; actual surface-format/present-mode negotiation; swapchain creation/resize/minimize; GPU memory and submission/presentation retirement; shaders and reflected layouts; bounded glTF cooking and runtime packages; actual PBR/shadow/transparent/post/UI rendering; queued inspector operations; visual review, performance fixture and independent final graphics acceptance. `render_probe` submits no frames and therefore cannot satisfy those gates.

See [Windows checks](windows-checks.md). M2-B through M2-F remain behind the prerequisite acceptance gates rather than being filled with placeholder implementations.
