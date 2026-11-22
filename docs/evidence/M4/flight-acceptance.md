# M4 connected-system flight acceptance

The fixed flight replay crosses from a protected NPC starter system to a lawless system. Security applies to the whole system. On arrival the normal flight demo draws the destination's streamed landmark alongside the unchanged frigate and asteroid workload. The public green triangle is synthetic evidence, not gameplay art or a collider. The fixture reuses the authored asteroid contacts and does not model the production galaxy balance or policing.

## Reproduce

Build the `demo-release` preset and supply the verified local frigate package and a public board marker cooked using the [standalone project guide](../../creator-project.md). Create a new corrupt fixture file containing exactly `not a checked package` followed by a newline. Use new report paths:

```sh
build/demo-release/flight_demo "$frigate" --replay --frames=600 --warmup=60 \
  --width=1280 --height=720 --mute --report=control.csv
build/demo-release/flight_demo "$frigate" --replay --crossing --frames=600 --warmup=60 \
  --width=1280 --height=720 --mute --destination="$marker" --report=crossing.csv
build/demo-release/flight_demo "$frigate" --replay --crossing --frames=600 --warmup=60 \
  --width=1280 --height=720 --mute --destination="$corrupt" \
  --destination-retry="$marker" --report=recovery.csv
```

`--crossing-hold` injects an occupied berth, stages corrected contacts after six queued blocked frames and proves admission on the same route. Use `--validation --motion-vectors --crossing-hold` for the separate correctness run, not the warmed timing comparison.

- Frigate SHA-256: `99c996f33d3cc98dbd801e3aa302ee2a5894d0fa6bcfdd18f59b73e72e74d834` (private input, not distributed).
- Cooked public marker SHA-256: `fab1e8b4072a47b86fd121b1485130ab9d13402a600fee61db6716449a13746f`.
- Corrupt file SHA-256: `7e08e291404b2a84b0aaecb941cddc07715be788dd46f25d6bc1235b0a268c01`.

## Evidence

Measured source `9cbc604`, integrated without tree changes by signed [PR #128](https://github.com/kyaulabs/ngen/pull/128) at `26eaa35`. Linux Clang 23.1.5 Release, Iris Xe (TGL GT2), Vulkan 1.4 driver `109064193`, Wayland. The flight replay ran with the 249,942-triangle authored workload plus the destination marker. Each scenario ran three times after 60 warmup frames, recording 600 frames per run. Values below are the worst observed per-run statistic within each scenario; they are not pooled percentiles.

| Scenario | Frame median | Frame p95 | Frame p99 | Longest host queue gap |
|---|---:|---:|---:|---:|
| No crossing control | 16.411 ms | 29.750 ms | 35.363 ms | 43.557 ms |
| Healthy crossing | 16.497 ms | 32.504 ms | 35.431 ms | 44.347 ms |
| Corrupt then successful retry | 16.276 ms | 27.709 ms | 34.215 ms | 45.186 ms |

See the [per-run timing table](flight-crossing-timing.csv) and the [healthy and fault trace excerpts](flight-crossing-trace.csv). During transit and blocked exit the CSV's local snapshot tick is intentionally frozen; the exit and arrival frame indexes come from the journey gate, not repeated local tick values.

`queue_ns` is the host observation of successful queueing. Consecutive values cover stalls between queues; they do not measure display scan-out. The flight-frame and 200 ms hitch thresholds held on this fixture.

All three healthy runs advanced upload during transit and arrived on the first exit frame (8). All fault runs kept rendering during the corrupt-file rejection and arrived at frame 17 after one valid retry. An additional Vulkan-validation replay with motion vectors rendered 15 blocked-exit frames, including six occupied-berth frames, then arrived at frame 23. The same fault case passed a strict ASan/UBSan run with leak detection enabled. The Release demo graph passed 28/28 CTests; the headless ASan/UBSan flight graph passed 9/9. The private capture itself is not distributed; the native test checks green destination pixels. Separate [scene-probe evidence](crossing-replay.md) exercises a delayed missing file, resource-record pressure and eviction; that is not physical VRAM exhaustion.

Largest process RSS across the measured crossings was 385,948 KiB (`wait4`). DRM `fdinfo` observations at roughly 5 ms, deduplicated by device/client, reached 312,696,832 bytes resident and 314,798,080 bytes total. Maximum frame-sampled tracked allocations were 234,608,524 bytes. Separate accounting from an integrated GPU may include overlapping pages; do not add the numbers or infer an unobserved physical memory peak.

Separate read-only physics and renderer reviews found premature exit-only upload, abort-on-collision, missing landmark history, and insufficient phase evidence. The changes advance upload during transit, keep the proxy on failure, permit a bounded upload retry, recover from an injected occupied berth, and emit explicit queued exit and arrival frame indexes. The follow-up did not identify a remaining blocker for the fixed fixture. Arbitrary collision replacement, production catalogs and physical scan-out were outside the review.
