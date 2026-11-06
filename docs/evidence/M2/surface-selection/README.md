# Surface selection and live query verification

This M2-A slice extends the startup context with a refreshable surface query and an owning value plan. It is based on `4d98647` and remains short of swapchain creation or frame presentation. `inputs.sha256` identifies the reviewed sources and tested binaries; `files.sha256` identifies retained output. Fingerprints identify the exercised artifacts, not replacement builds or byte-reproducible binaries.

`select_surface` consumes queried capabilities, format/mode spans and a drawable size. It retains no borrowed storage. The plan selects nonlinear sRGB (BGRA preferred, RGBA supported), FIFO presentation, a bounded-by-WSI image count, the current supported transform and a supported alpha mode. Fixed extents override requested sizes; variable extents clamp to advertised bounds. Zero drawable/surface extents suspend instead of producing an invalid swapchain request. Bad bounds/counts/layer capacity, unsupported usage/formats/modes/alpha or an invalid transform return explicit failures. Incrementing the requested image count cannot wrap.

`Context::inspect_surface()` refreshes native capabilities and drawable size on the owner thread, or returns a closed/suspended result before using expired handles. Native queries bound format/mode storage to 256/64 entries, respect the returned counts, propagate Vulkan query failures and verify color-attachment support for the selected format. Device selection applies the same surface policy before accepting a candidate. This does not replace the separate physical-device baseline checks. The caller must refresh after resize/scale changes; a plan is not a reservation or an owning Vulkan resource.

| Check | Result |
|---|---|
| Public policy tests plus existing startup failure test | 10 cases, 67 assertions passed |
| Clang 23.1.4 Debug, Release, ASan/UBSan | All seven CTest entries passed in each configuration |
| GCC 16.2.1 Debug | All seven entries passed |
| Affected implementation/header consumers, clang-tidy | Passed in all three Clang configurations |
| Required clang-format 18 | Passed |
| Default graphics-free CPU graph | Four CTest entries passed |
| Actual Intel/Wayland query | All four configurations passed |
| Actual validation calibration | All four passed with exactly one labelled intentional error |
| Injected surface loss, instrumented runtime | Expected exit 1 with original Vulkan result; no sanitizer diagnostics |
| Excessive native format count, instrumented runtime | Expected exit 1 before fill; no abort or sanitizer diagnostics |

Each successful native query selected `1280x720`, `VK_FORMAT_B8G8R8A8_SRGB` (50), `VK_COLOR_SPACE_SRGB_NONLINEAR_KHR` (0), `VK_PRESENT_MODE_FIFO_KHR` (2), and four images on the recorded Iris Xe/Wayland path. The probe also verifies that a post-close query returns the closed result. These are capability/selection observations: no image was acquired, rendered or presented.

The successive `red-*.txt` files record actual failed expectations before each policy change, including the initial real-GPU query attempt with a closed-result stub. The initial unit failures use Catch2's exit 42, not a build failure. Some intermediate compiler checks also caught enum initialization and signed flag conversions; those were corrected without suppressions. The final build/lint logs supersede those intermediate build attempts, not the retained behavioral failures.

`query-failure.cpp` is a process-local boundary fixture, not a production code path. It returns `VK_ERROR_SURFACE_LOST_KHR` for the capability query, or, under `NGEN_EXCESS_FORMATS`, reports 257 formats and aborts if the renderer attempts a fill. Both instrumented runs return the expected ordinary failure rather than continuing or aborting. This is injected evidence, not a claim that the hardware suffered surface loss.

The native checks use the existing isolated validation layer and process-local `LD_BIND_NOW=1`; instrumented checks additionally set `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`. Reproduce the normal paths with the [renderer presets](../../../../recipes/sdl/README.md). Build a fault fixture and run it only in that process:

```sh
clang++ -std=c++20 -shared -fPIC \
  docs/evidence/M2/surface-selection/query-failure.cpp \
  -o build/surface-check/query-failure.so
LD_PRELOAD="$PWD/build/surface-check/query-failure.so" \
  build/renderer-asan/render_probe
```

Add `-DNGEN_EXCESS_FORMATS` when compiling the count-limit variant. The expected exit is 1. Keep the normal runtime/validation environment above; no environment, driver, registry or installed library is changed persistently.

Same-session review covered span/value lifetimes, zero-size admission, checked count arithmetic, flag types, bounded enumeration, device rejection, error ownership and closed-handle short-circuiting. Builds cover all affected owned targets; AST checks include the context, surface policy, probe and both startup/policy test translation units. No actionable defect remains from that review, but it is not independent acceptance. Current Windows execution, native minimize/resize behavior, swapchain recreation/presentation retirement and all later M2 outcomes remain unverified here. No timing or memory-plateau claim is made.
