# GPU resource ownership and retirement

Status: **Linux and [Windows/NVIDIA resource execution](../windows-resources/README.md) verified; independent/formal acceptance and M2 remain open.** Source inputs are the resource changes following `f86f1c9`; `inputs.sha256` identifies the implementation. This is real buffer/image transfer alongside clear-only presentation, not rendering those images, the frigate, PBR or displayed-pixel verification.

The optional renderer adds VMA 3.3.0, recipe revision `5393b5bc9a6b8a2fb9612df877706608`, and overrides its Vulkan-Headers range with 1.3.296.0, revision `4ea73880426698c2e903c3e7acd892dc`. Existing SDL pins and `conan.lock` are unchanged. One private implementation TU compiles VMA with `VMA_VULKAN_VERSION=1003000`. Vulkan's platform loader/SDK discovery remains separate. VMA supplies suballocation and statistics rather than another NGEN allocator. Its v3.3.0 source archive SHA-256 is `c4f6bbe6b5a45c2eb610ca9d231158e313086d5b1a40c9922cb42b597419b14e`. Unmodified package notices/license texts are retained in `licenses/`; distributions containing these dependencies must retain the applicable notices. NGEN's license is unchanged.

Context owns allocator/native objects. Move-only buffers/images weakly borrow generation-checked slots; moved-from, foreign, stale and closed owners reject access. Images currently support only single-mip/layer/sample 2D RGBA8 sRGB/UNORM. Their usage must support the owned view. Native handles are borrowed on the owner thread, not asynchronous leases. Submit resource uses through the tracked Context operations; future draw binding must join that tracking before using these objects. The optional native `TimelineWait` is a trusted integration/test boundary: the caller supplies a valid same-device timeline, legal value and lifetime through completion. Opaque resource/submission identity checks do not validate arbitrary native handles.

Admission covers **64 shared buffer/image records, two transfer contexts and 256 MiB of allocation bytes, including retiring allocations**. Requests validate ranges, dimensions, format/usage limits and driver memory requirements before native allocation, with a post-allocation aligned-size check. VMA reserved blocks, WSI and other driver memory are separate; this is not a hard total-VRAM cap. Successful submission alone publishes serials, last use and queued layouts. Collection queries the resource timeline and acquires newly observed completion with a zero-timeout host wait before CPU access/reclamation. WSI retirement remains separate. Context shutdown proves completion before destroying resources/device; persistent backend errors may retain native ownership. Stop normal use after a shutdown failure and retry close; general recovery is not established.

| Native configuration | CTest | Resource clear requests / generations / resizes | Image replacements | Allocation / reserved plateau |
|---|---|---|---|---|
| Clang Debug | 7/7 | 96 / 3 / 2 | 375 | 8,348 / 3,145,728 bytes |
| Clang Release | 7/7 | 96 / 3 / 2 | 351 | 8,348 / 3,145,728 bytes |
| Clang ASan/UBSan/leak detection | 7/7 | 96 / 3 / 2 | 477 | 8,348 / 3,145,728 bytes |
| GCC Debug | 7/7 | 96 / 3 / 2 | 549 | 8,348 / 3,145,728 bytes |

All four configurations also passed separate startup, intentional callback calibration and original first-light scenarios. The instrumented SDL repair remains active. Resource probes verify exact buffer and 140-byte image roundtrips, timeline-gated pending retirement/reclamation, two-context backpressure, slot exhaustion/reuse, 128 buffer replacements, repeated image transfers/replacement across presentation/resize, foreign owner/submission rejection, invalid requests and close invalidation. Replacement counts vary because the bounded callback runs before frame attempts, including skipped attempts. They are not performance measurements. Wayland reports `minimize_verified=0`; Windows minimization is a separate owner gate.

`host-acquire.cpp` observes two device/timeline identities. Final runs report zero missing acquisitions, zero blocking host waits and zero queue idle calls. Two device-idle calls belong to the explicit teardown of the temporary foreign-context check and main context, not normal transfer/resize. `verify-native.sh` records the commands, with native libraries built in ignored `build/` paths. Synchronization validation is required by the retained first-light observer. The shared presentation loop preserves the existing frame/resize checks.

`ngen-resource-fault-status.txt` records **21 controlled ASan/UBSan/leak-detection scenarios**, each rejected with exit 1 and no unexpected validation/sanitizer report: memory/buffer/image allocation, command buffers, initial/partial/foreign-context pools, timeline creation, image views (including replacement with pending old-image work), both bind paths, queue submission, pre-allocation buffer/image budgets, unsupported/per-format image limits, host acquisition failure and shutdown retry. Idle retry proves no device destruction before retry; it does not simulate actual device loss. `verify-faults.sh` and the four local fixtures retain the controls; allocation/idle/WSI fixtures are reused from `../presentation-first-light/`. The image-limit fixture synthetically narrows reported format extent to 4×4; this is a boundary test, not a claimed compliant hardware profile.

Retained Red logs capture missing lifetime/copy/image implementations, absent pending work, missing host acquisition, missing pre-allocation admission, ignored per-format limits, transfer-only view creation and missing Windows resource execution. Final positive logs are the current Green. Review fixed the view issue after reproducing `VUID-VkImageViewCreateInfo-image-04441`; usage rejection now precedes native image creation. Initial fixture setup failures are retained separately: preloading an instrumented DSO into non-ASan `timeout` failed to resolve its runtime; placing `env LD_PRELOAD=...` after `timeout` fixes process scope. The first bind fault hook targeted unused `*Memory2` calls; reading VMA's actual default path identified legacy binds, whose injections now fire and clean up. Neither setup attempt counts as a product fault pass.

Strict owned-code clang-tidy passed **11 TUs in each of Clang Debug/Release/ASan**, including implementations, shared probe support, both probes and affected header consumers. Real compile databases are identified in `compile-db.sha256`; source/configuration commands and tool identities are recorded separately. GCC builds/runtime pass, but clang-tidy cannot parse the actual GCC module dependency flags; its unmodified failure is retained rather than filtering metadata. Default owned analysis excludes system dependency headers. A separate VMA-header analysis produced **107 diagnostics promoted to errors**; `ngen-resource-vma-vendor-tidy.txt` is an advisory baseline, not a clean vendor claim. Earlier SDL vendor findings remain in their existing evidence. No warning/sanitizer/validation suppression was added.

CPU Debug remains 4/4, PowerShell process-boundary tests cover resource log capture, failure short-circuiting and runtime restoration, and pinned clang-format 18 passes. The source/ownership review is same-session, not independent/human acceptance. Hosted jobs build/analyze the new TUs but have no display for this GPU workload. The owner's Windows MSVC/clang-cl Debug/Release runs now verify this source separately. Broader driver/surface recovery, shader layouts, cooked assets, rendered pixels and performance remain unverified.

To reproduce after the ordinary renderer preparation/configuration, compile these Linux-only observers (the loader and headers must be available). Sanitized preload libraries must enter only the instrumented executable, not `timeout`:

```sh
mkdir -p build/resource-check build/surface-check
E=docs/evidence/M2/gpu-resources
P=docs/evidence/M2/presentation-first-light
for item in presentation-observer require-sync-validation; do
  clang++ -std=c++20 -shared -fPIC "$P/$item.cpp" -o "build/surface-check/$item.so" -ldl
done
for item in host-acquire image-limits; do
  clang++ -std=c++20 -shared -fPIC "$E/$item.cpp" -o "build/resource-check/$item.so" -ldl
done
san_observer() {
  clang++ -std=c++20 -shared -fPIC -fsanitize=address,undefined -fno-sanitize-recover=all "$@" -ldl
}
san_observer "$E/native-failure.cpp" -o build/resource-check/native-failure-asan.so
san_observer "$E/admission.cpp" -o build/resource-check/admission-buffer-asan.so
san_observer -DNGEN_IMAGE_ADMISSION "$E/admission.cpp" -o build/resource-check/admission-image-asan.so
san_observer -DNGEN_UNSUPPORTED_IMAGE "$E/image-limits.cpp" -o build/resource-check/image-unsupported-asan.so
san_observer -DNGEN_FAIL_HOST_WAIT "$E/host-acquire.cpp" -o build/resource-check/host-wait-failure-asan.so
san_observer "$P/allocation-failure.cpp" -o build/resource-check/allocation-failure-asan.so
san_observer "$P/idle-retry.cpp" -o build/resource-check/idle-retry-asan.so
sh "$E/verify-native.sh"
sh "$E/verify-faults.sh"
```

The ASan executable uses the existing build-local non-PIE workaround; no system policy or environment was changed. Normalized logs replace repository/cache roots and strip terminal color/trailing diagnostic whitespace. Licenses remain byte-identical to package notices. `files.sha256` covers this retained evidence; hashes establish identity, not reproducible binaries.
