# Restrained bloom and combined workload

The fixed workload now includes bloom, the live queued inspector and repeated
checked-package replacement. **M2 acceptance remains open:** changed-tree
Windows/NVIDIA execution, independent graphics review and personal owner visual
acceptance are still required. The completed `ef984ef` Windows execution remains
valid for its original scope and needs no duplicate run.

Bloom is fused into the existing post pass: normalized Gaussian weights, a 9x9
footprint and 25 bilinear samples of reconstructed linear HDR above threshold 1.
The smooth center estimate is reused across FXAA taps, before exposure/tone
mapping. UI composites afterward and is not bloomed. `gpu_post_ns` includes
bloom/tone/FXAA; there is no fictitious separate bloom interval, scratch image,
new pipeline or GPU owner. Generic APIs default to zero; the workload defaults
to 0.04 and supports `--no-bloom`. Finite strength admission is 0..0.25; the
inspector offers a restrained 0..0.1 slider through next-boundary commands.

Post constants grow from 16 to 32 bytes, `PostDraw` from 64 to 80, preserving
explicit offsets/padding and the Vulkan 1.3/SPIR-V 1.5 baseline. Nine finite shader
entries remain offline validated/reflected/embedded. No dependency was added.

Actual verification: four Clang Debug/Release/ASan+UBSan and GCC Debug graphs
pass 12/12 CTests, physical resource/material/UI probes, 5/5 workload and 4/4
inspector process cases. Controls cover independent Gaussian radiance, threshold,
symmetry/support, linear exposure, single sRGB encoding, disabled identity,
malformed strengths, queued WSI and bloom-before-UI clipping/order. Headless TSan
passes 6/6; shipping-disabled 11/11; shader-disabled 10/10. Actual affected Clang
analysis covers 119 target/configuration invocations with subsequent Main/UI
reruns. GCC module-flag limits and existing asset-vendor advisories remain
separate. Ordinary validation, leak checks and sanitizer instrumentation stay on
for correctness runs; they are off for the optimized measurements below.

The automated walkthrough demonstrates edits, verifies them next frame and
restores exposure 1/FXAA on at frame 3, rather than benchmarking with FXAA silently
disabled. Replacements use real SDL keyboard input, one CPU reader and tracked
owner-thread GPU preparation. A 30-frame settling interval separates completed
reloads. Reports include per-frame VMA allocation/reservation/live/retiring
counts, owning generation, reload state, native surface extent and host clock.

| Scoped optimized Iris Xe run | Steady UI+bloom | Four same-file reloads |
|---|---:|---:|
| Measured/warmup frames | 600/60 | 600/60 |
| Native extent for every measured frame | 1280x720 | 1280x720 |
| Frame P50/P95/P99 | 18.645/33.603/42.422 ms | 19.736/41.963/75.688 ms |
| Maximum frame | 47.555 ms | 100.715 ms |
| Sampled process DRM peak | 294,305,792 bytes | 531,017,728 bytes |
| Cold-inclusive Linux peak RSS | 349,436 KiB | 540,532 KiB |

Both runs meet the scoped 50/66.7/100 ms percentile, 200 ms maximum, 1 GiB process
DRM and 6 GiB Linux residency checks. Steady C++ `new` and ImGui allocation calls
are zero; streaming allocations remain visible, not reclassified as zero.
The 246,080 triangles, 100 real instances, 1024 shadow and 0.04 bloom are retained.
Exact CSVs, source/input/settings/executable identities and private capture hashes
are retained; model/package/BMP bytes are not committed.

DRM sampling deduplicates i915 client IDs and sums region totals, including shared
objects/WSI/driver allocations outside VMA. It measures this process, not global
whole-device usage. Sampled peaks are not hard upper bounds. VMA allocation,
reservation, driver residency and CPU RSS are distinct; Linux RSS is not Windows
working set. Original identity hashing warms file cache: these are same-file
warm-cache reloads, not cold-storage latency guarantees. Parent CLOCK_MONOTONIC
and child steady-clock correlation is checked, not assumed.

Tracked allocation/reservation return to 211,566,728/214,396,216 bytes, 22 live
records and zero retiring records after each replacement. Settled DRM totals are
about 270.1 MB. CPU settled RSS is not constant across generations; retained
records expose allocator/residency variation rather than claiming an exact flat
CPU plateau or unbounded-run leak proof. Four replacements stay within the
reported cold-inclusive process peak. Longer/platform-specific memory and owner
acceptance remain distinct gates.

An earlier combined resize/reload run **failed** the all-frame 200 ms assertion:
frame 44 took 256.820 ms, including 254.597 ms in presentation. Native boundary
tracing attributes the recurring frame-24/44 stalls to image-acquisition polling
after swapchain resize. Suppressing reloads preserved resize stalls; suppressing
resizes allowed three actual reloads with a 45.949 ms maximum. Diagnostic controls
intentionally exit 1 because they omit required walkthrough actions; they are
not clean validation/performance passes. The failed run and controls are retained.

Numeric streaming acceptance now uses `--no-resize`, checks all 600 native extents
are 1280x720, and retains the same strict 200 ms assertion. Default walkthroughs
still verify two resizes, and the 256.8 ms resize/compositor latency is explicitly
unresolved, not hidden or claimed fixed. Arbitrary driver/display/I/O deadlines
and scanout are not proven. This distinction preserves the fixed-resolution
streaming requirement without silently exempting slow frames inside that run.

```sh
sh tools/prepare.sh inspector-release
cmake --preset inspector-release
cmake --build --preset inspector-release
build/inspector-release/scene_workload scene.ngas --inspector --frames=600 \
  --warmup=60 --capture=new.bmp --report=new.csv
build/inspector-release/scene_workload scene.ngas --inspector --walkthrough \
  --no-resize --reload-cycles=4 --frames=600 --warmup=60 --report=new-stream.csv
```

`measure.py` records Linux wait4/proc/DRM observations using fresh ignored outputs;
`latency.cpp` is diagnostic-only system-boundary instrumentation. Neither creates
an alternate renderer or executes returned evidence. Source review is same-session,
not independent acceptance. See `review.txt`, `sources.sha256`, invocation-specific
compile metadata and retained failures before interpreting the budget results.
