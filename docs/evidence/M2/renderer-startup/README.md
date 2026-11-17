# Locked renderer startup verification

Verified the task diff over `7e982de` on `feat/kyau-7d227e-render-foundation`. This records the original M2-A dependency/startup slice, not M2 completion or independent acceptance. Current results are in the [pinned SDL repair](../wayland-repair/README.md).

`conanfile.py` now has an optional `renderer=True` graph. `conan-render.lock` pins its recipe revisions separately from the unchanged CPU `conan.lock`. SDL 3.4.16 is the available Conan candidate actually built here; the previous system-SDL result used 3.4.18. This choice is explicit, not a claim that the plan's newer candidate was available or verified through Conan. SDL is already required by the startup code; no new framework was introduced.

SDL is shared, with video/Vulkan enabled and unused audio, GPU/2D-renderer, camera, joystick, haptic, HID, power, sensor, dialog and tray subsystems disabled. OpenGL/OpenGL ES/DirectX are disabled. Linux uses Wayland, not X11, with SDL's D-Bus/udev integrations disabled; Windows uses Win32. X11 support is not claimed. Both Windows compiler graphs resolve the pinned SDL recipe offline after preparation; neither Windows binary was built on Linux.

A static-SDL attempt failed to link because the recipe detected host libdecor without propagating its static link requirement. Shared SDL retains window decorations and avoids that unresolved static dependency. Linux still uses host EGL/libdecor and compositor facilities; Vulkan headers/loader come from the installed SDK/system. Recipe locks do not pin those host libraries or promise hermetic/byte-identical binaries. No system package, driver, registry or security setting was changed.

Conan's `VirtualRunEnv` supplies SDL's DLL search path. `tools/build-windows.ps1 -Renderer` activates it per configuration, runs callback calibration followed by clean startup, restores the environment even on failure and packages both logs. The default CPU build remains graphics-free. No SDK optional SDL bundle is needed.

Local Linux Clang 23.1.4/libstdc++ C++20 results:

| Check | Result |
|---|---|
| Locked SDL graph, Debug and Release configure/compile/link | Passed |
| Debug CTest | 5/5 passed |
| Release CTest | 5/5 passed |
| ASan/UBSan owned-code build and headless CTest | 5/5 passed |
| Intel Iris Xe/Wayland clean startup, Debug and Release | Passed, no validation messages |
| Explicit callback calibration, Debug and Release | Passed, exactly one intentional message |
| PowerShell native/input/runtime-environment tests | Passed |
| PowerShell renderer helper with real local Vulkan and generated Conan environment | Passed; not Windows evidence |
| Default CPU graph/build/CTest | Passed, SDL absent, 4/4 tests |
| clang-tidy on context/probe/startup-test TUs, all three configurations | Passed |
| Formatting of changed owned C++ sources | Passed |
| Actual ASan/UBSan Wayland GPU startup | **Failed: 480 bytes in five Wayland proxy allocations** |
| Windows renderer build/startup | Not run; owner handoff pending |

The GPU sanitizer failure is retained in `asan-gpu-leak.txt`. `sdl-lifetime.cpp` reproduces the same leak with only `SDL_Init(SDL_INIT_VIDEO)` and `SDL_Quit()`, without NGEN or Vulkan (`sdl-only-leak.txt`). The existing system-SDL 3.4.18 renderer also reports the same leak (`system-sdl-leak.txt`). These controls isolate it below NGEN's ownership code, but do not establish an upstream fix. At this revision it blocked graphics acceptance; no leak suppression, disabled detector or dependency version workaround was applied. Passing headless CTest does not clear it. Dependencies themselves were not rebuilt with sanitizers.

`render_probe --check-validation-callback` submits one application-labelled error through `vkSubmitDebugUtilsMessageEXT`, verifies it was counted, and checks that close remains failed and borrowed handles clear. Its intentional diagnostic is not a clean validation pass. Normal startup separately requires zero recorded errors and successful shutdown.

Review found that the new calibration mode could mask a device-wait failure because both validation errors and `vkDeviceWaitIdle` failure make `close()` false. A process-local Vulkan-boundary interposer reproduced exit 0 with `VK_ERROR_DEVICE_LOST`. The probe now also checks the sticky `Context::close_result()`: the same injected failure exits 1, while ordinary calibration and startup exit 0. `wait-idle-failure.cpp` and `wait-idle-failure.txt` retain the fixture/result. This is simulated API failure, not actual driver-loss or in-flight retirement evidence.

Reproduction, from the repository root with Conan on PATH:

```sh
conan install . -pr:h profiles/linux-clang -pr:b profiles/linux-clang -o '&:renderer=True' --lockfile conan-render.lock --build=missing -of build/render-conan
cmake -S . -B build/render-conan -G Ninja -DCMAKE_TOOLCHAIN_FILE="$PWD/build/render-conan/conan_toolchain.cmake" -DCMAKE_BUILD_TYPE=Debug -DNGEN_BUILD_RENDERER=ON -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build/render-conan -j2
ctest --test-dir build/render-conan --output-on-failure --no-tests=error
# Requires a real display and visible Khronos validation layer:
build/render-conan/render_probe --check-validation-callback
build/render-conan/render_probe
```

For Release, use a fresh directory and set both Conan host/build `build_type=Release` and CMake `CMAKE_BUILD_TYPE=Release`. The sanitizer configuration uses the Debug packages with `NGEN_SANITIZER=address,undefined` and `CMAKE_EXE_LINKER_FLAGS=-no-pie`; execute with `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1`. The isolated local validation-layer paths are documented in [the evidence index](../README.md).

To reproduce the shutdown-boundary check, compile `wait-idle-failure.cpp` with `clang++ -std=c++20 -shared -fPIC ... -o wait-idle-failure.so`, then set `LD_PRELOAD` to that absolute path for the Debug probe's calibration invocation only; expected exit is 1. No loader, SDK or driver file is replaced. To reproduce the dependency leak, compile `sdl-lifetime.cpp` with ASan/UBSan against the generated SDL include/library paths and run it in the same Wayland session.

Same-session source/contract review covered the changed probe/header consumers, DLL activation/restoration, compiler identity, CPU graph isolation and fail-closed process handling. It is not independent or human approval. At that point M1 formal acceptance, the Wayland sanitizer issue, surface/swapchain negotiation and all rendering/asset/performance gates remained open.

Later owner execution at `c730a18` passed Windows [MSVC](../windows-msvc/README.md) and [clang-cl](../windows-clang-cl/README.md) Debug/Release builds, tests and startup; the table above records this earlier slice, before those runs. The [Wayland follow-up](../wayland-tablet-leak/README.md) identifies unreleased tablet/pad proxies in a current SDL-only reproduction. That investigation does not replace the retained five-allocation report. A subsequent owner-approved [Linux SDL repair](../wayland-repair/README.md) now passes the instrumented SDL-only fixture, seven headless tests and actual GPU startup/calibration; Windows keeps the upstream recipe. The earlier failure table above is historical, not the repair's current result.
