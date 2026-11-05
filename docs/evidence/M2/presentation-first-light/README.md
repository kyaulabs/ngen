# Clear-only presentation first light

This slice, based on `4811662`, adds actual swapchain acquisition, dynamic-rendering clears and queued presentation. It does not complete M2-A or M2. Windows/NVIDIA execution of this code, visual captures, VMA resource retirement, shaders, cooked assets, PBR, the inspector, performance and independent acceptance remain open. `inputs.sha256` identifies the exercised sources, executables and boundary libraries; `files.sha256` identifies retained evidence. These are artifact fingerprints, not a reproducible-binary claim.

`Context::Options::presentation` explicitly enables the maintenance presentation profile. Startup-only callers keep the earlier requirements. Presentation requires surface maintenance, capabilities2, swapchain maintenance and its queried feature. Validation-enabled presentation additionally requests synchronization validation through `VK_EXT_validation_features`. The implementation uses EXT names/types supported by the Vulkan 1.3-era hosted headers; it does not require the newer KHR aliases or use present-wait.

The context owns two frame contexts and at most sixteen swapchain-image records. Images are borrowed from WSI; views, command pools/buffers, acquisition/submission fences, rendering-complete semaphores and maintenance presentation fences are owned. Acquisition uses a fence, retains unfinished acquisition across nonblocking polls and completes before GPU image access. Submission completion alone is not presentation retirement. Every presentation carries a maintenance fence, and reuse/recreation waits for the relevant operation before resetting or destroying its objects. Ordinary frames and resize do not call device idle. Shutdown can wait. A transient idle/retirement failure keeps ownership for a retry and remains sticky after successful cleanup.

`render_probe --check-presentation` opens a visible window, queues at least 96 accepted frames, requests 800×600 and then 1280×720, and requires two observed resizes. It checks SDL logical/pixel dimensions against a later completed swapchain generation, not just the success of a resize request. The loop drains at most 256 events per iteration and has a ten-second deadline. On the Windows SDL driver it also requires observed minimize, skipped rendering while minimized and observed restore. Wayland does not run that Windows state check; its output explicitly reports `minimize_verified=0`. Closed-context frame calls are rejected without using expired native handles. A queued outcome is not proof of scanout or pixel correctness.

| Check | Observed result |
|---|---|
| Clang 23.1.4 Debug, Release, ASan/UBSan | Builds and all seven enumerated CTest entries passed in each; none skipped |
| GCC 16.2.1 Debug | Build/link and all seven entries passed |
| Six affected implementation/header consumers, clang-tidy | Passed in all three Clang configurations |
| GCC compilation database through clang-tidy | Unavailable: GCC module-scanning flags are rejected; full failed report retained, no metadata filtering |
| Required clang-format 18 | Passed after formatting the new idle-retry fixture |
| Default CPU graph | Four CTest entries passed; remains graphics-free |
| PowerShell runtime/failure boundaries | Passed, including presentation invocation, captured output, short-circuiting and environment restoration; not Windows binary execution |
| Intel/Wayland, all four native configurations | 96 accepted frames, three swapchain generations, two verified resizes, no validation diagnostics |
| Native boundary observer, all four | Every presentation fenced; no device idle before shutdown; extents 1280×720 → 800×600 → 1280×720 |
| Baseline startup/calibration, all four | Startup clean; calibration emits exactly one labelled intentional error |
| Instrumented partial initialization/recreation | Six injected allocation failures exit 1 without validation or sanitizer diagnostics |
| Instrumented retirement wait/idle retry | Two injected failures exit 1, preserve the error and clean up after retry; no validation or sanitizer diagnostics |
| Missing maintenance feature / excessive image budget | Explicit exit 1; no validation or sanitizer diagnostics; startup-only profile still succeeds without maintenance |
| Suboptimal acquisition plus delayed completion | Red: zero accepted frames; repaired instrumented run: 96, no starvation or sanitizer diagnostics |
| Out-of-date presentation accounting | Red: caller counted 96 but boundary accepted 95; repaired instrumented run: 96 accepted |
| Continuous event stream | Red: external three-second timeout (124); repaired instrumented run completes 96 frames and both resizes |

The ASan/UBSan runs use the already instrumented Linux SDL dependency graph, leak detection and fail-fast settings. The normal compositor, loader and driver are not all instrumented. No suppressions, disabled leak checks or mutated stock cache packages were used. An initial final-check batch hit its tool timeout during ASan analysis; the complete ASan analysis was rerun successfully. The first workflow-syntax command used a Python interpreter without YAML support; validation succeeded with the existing Conan virtual environment instead. Neither attempt was treated as a pass.

The retained boundary fixtures are Linux-only, process-local tests of public SDL/Vulkan calls, not production fallbacks. `require-sync-validation.cpp` checks the instance's requested synchronization-validation feature. `presentation-observer.cpp` counts real accepted presents, swapchain creation and supplied presentation fences, and rejects early device idle. `suboptimal-acquire.cpp` reports suboptimal after real acquisition and delays one status poll per acquisition. `present-outdated.cpp` reports one out-of-date result after real presentation queue operations, preserving actual semaphore/fence behavior. `event-flood.cpp` preserves native event processing while supplying an endless user-event stream. `profile-limits.cpp` hides maintenance support, or reports a 17-image minimum under `NGEN_EXCESS_IMAGES`. `allocation-failure.cpp` injects `pool:2`, `fence:3`, `fence:7`, `view:2`, `semaphore:3`, `swapchain:2` or `wait:1`; allocation and wait failures are distinguished in the logs. `idle-retry.cpp` reports one idle failure after a real wait and verifies that device destruction happens only after the retry. These are controlled failures, not observed hardware faults.

Prepare/build with the [renderer presets](../../../../recipes/sdl/README.md), preserving the documented non-PIE sanitizer configuration. Set the local validation/runtime environment, then run:

```sh
export VK_LAYER_PATH="$PWD/build/validation-layer/usr/share/vulkan/explicit_layer.d"
export LD_LIBRARY_PATH="$PWD/build/validation-layer/usr/lib"
export LD_BIND_NOW=1
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
build/renderer-asan/render_probe --check-presentation

mkdir -p build/surface-check
clang++ -std=c++20 -shared -fPIC \
  docs/evidence/M2/presentation-first-light/allocation-failure.cpp \
  -o build/surface-check/allocation-failure.so -ldl
NGEN_FAULT_API=view NGEN_FAULT_AT=2 \
  LD_PRELOAD="$PWD/build/surface-check/allocation-failure.so" \
  build/renderer-asan/render_probe --check-presentation
# Expected exit 1, injected view:2 failure, no sanitizer or validation diagnostics.
```

Other Vulkan fixtures compile with the same command, substituting the source/output name. Compile `profile-limits.cpp` twice, adding `-DNGEN_EXCESS_IMAGES` for the budget variant. The SDL event fixture needs the locked SDL package's include directory. Load the observer and synchronization checker together with a colon-separated `LD_PRELOAD`; use a fresh process for each scenario. No persistent environment, registry, driver or installed library is changed. Hosted jobs build/test/analyze without a display and cannot provide this GPU evidence; current workflow results belong to PR #2.

Same-session review traced header consumers, partial construction, borrowed handle/callback lifetime, acquisition/submission/presentation state, semaphore reuse, overflow/budgets and shutdown retry. It fixed suboptimal-acquisition starvation, out-of-date success accounting, unbounded event draining and premature device destruction after an unproven idle result. The owning presentation member is destroyed before native-handle/callback-state subobjects; destruction detaches the callback even if shutdown cannot finish. No remaining actionable defect was identified in the exercised contract. This is not independent review or proof of arbitrary driver recovery. Persistent non-device-lost backend failures can prevent safe native reclamation; explicit close/retry while the context remains alive is required, and destructor fallback does not promise complete reclamation. Actual device loss, surface-loss recovery, arbitrary resize timing, Wayland minimization, Windows behavior, pixels and memory/performance plateaus remain unverified.

The normative lifetime basis is [maintenance presentation fences](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainPresentFenceInfoKHR.html) and [queue presentation/error semantics](https://docs.vulkan.org/refpages/latest/refpages/source/vkQueuePresentKHR.html). EXT structures are the compatible aliases used by this implementation. See the [Windows handoff](../windows-checks.md) for the next required execution.
