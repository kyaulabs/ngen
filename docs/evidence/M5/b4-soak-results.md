# B4 one-hour result

The [B4 protocol](b4-soak-protocol.md) passed on the declared laptop fixture.
From 21:03:12 to 22:03:13 UTC on 2026-11-25, one flight window ran for
3,600.025 seconds. It made 782 native arrivals and 391 committed sales,
reopening and checking both travel receipts after every sale. There were
195 victories, 196 losses and 97 exhausted charters. The final, partial
charter stopped with 9 ore at the station, 3 in the belt, 53 player credits
and vessel 3. Neither exact retries nor conflicting probes changed the
committed revision, ownership or balances. No gameplay server, account or
DB was used. The process ran in a fresh network namespace; Wayland used
local desktop IPC and the audio device remained unopened.

| Measurement | p50 | p95 | p99 | Maximum |
|---|---:|---:|---:|---:|
| Host frame, 185,467 post-warmup queues (bin upper bounds) | 16.9 ms | 34.9 ms | 37.7 ms | 85.224 ms |
| Berth-gated market operation, 391 | 0.454 µs | 1.050 µs | 1.746 µs | 21.552 µs |
| Save publication and directory sync, 391 | 3.645 ms | 10.839 ms | 14.261 ms | 14.935 ms |

The longest host queue gap was 85.221 ms. These are host timings, not display
scan-out. The market is a fixed local station transaction, not multiplayer or
a storage benchmark. Its p95 was below 20 ms without counting any filesystem
or replay time.

| Resource | Peak | Settled p95, min 5–10 | Settled p95, min 55–60 |
|---|---:|---:|---:|
| Process RSS, bytes | 227,573,760 | 227,573,760 | 227,119,104 |
| Renderer allocation, bytes | 234,608,524 | 234,608,152 | 234,608,152 |
| Renderer reserved, bytes | 243,852,488 | 243,852,488 | 243,852,488 |
| Live GPU resource records | 30 | 27 | 27 |

There were 3,601 memory samples and 288 comparable settled samples in each
window. The tracked retiring count peaked at zero in sampled frames. These
allocator bytes are not the system-wide GPU working set; do not add them to
RSS on shared-memory hardware.

## Reproduce and verify

Use [the raw tagged CSV](b4-soak-trace.csv):

```sh
python3 tools/b4_report.py docs/evidence/M5/b4-soak-trace.csv
```

- Source: signed `05771b04c13395d4290ac043e9a5ee23e09dcb22`.
  Executable SHA-256: `e46c381780bab2b2defee26772e20c725110876cd42ee7033cef6f21d12a8757`.
- X1 Carbon with i7-1185G7 (4 cores, 8 threads available), 15.32 GiB reported
  RAM, Linux `7.2.15-hardened1-3-hardened`, Clang 23.1.5, CMake 4.5.1, optimized
  Release, 1280×720, no Vulkan validation in the timed run. The cgroup CPU
  quota file was unavailable. AC was online with the powersave governor;
  60 one-minute CPU0 clock/thermal samples ranged 0.493–3.600 GHz and 49–71 °C.
- Intel Iris Xe (TGL GT2), device UUID `8680499a-0100-0000-0002-000000000000`,
  Mesa `26.3.1-arch1.1`, Vulkan 1.4 driver `109064193`, Wayland.
- `conan-demo.lock` SHA-256
  `f9e452d82ceac5743c86f574b0691d1867a14974d6c9905d7c9142066105570a`;
  Jolt `5.6.0@ngen/headless-double#67a3b4c0d2e06dc7514a970066ac5074`, SDL
  `3.4.16@ngen/wayland#6a35b483f98d9692e69a3b88780f8ac2`, and pinned Lua 5.5.1
  static library SHA-256 `76054b59f6ef6b8d7e6a0081a7b0cf9fd048ac55eafebf8de9bec8d83d0ec5a8`.
- The private frigate and public landmark matched the SHA-256 hashes in
  [M4 flight acceptance](../M4/flight-acceptance.md); streaming seed `19`.
  The benchmark carries no assets, saves, binaries or recording.

Shorter Vulkan-validation and strict ASan/UBSan/LSan runs exercised the
workload separately from timing. Native save and script tests rejected damaged
saves, malformed rules and instruction/heap overruns. The one-hour test is
technical evidence, not the independent gameplay/economy review or Sean's
recorded interactive playthrough. Those two gates remain open.
