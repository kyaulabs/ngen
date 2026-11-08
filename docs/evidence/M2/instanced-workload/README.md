# Instanced ship workload

**M2 remains open.** Source `a346c29ae95596d16cf8623f68a5396b2a095432`
implements the ship-plus-100-asteroid workload and bounded GPU timing diagnostics.
Its four hosted jobs passed in run `42412652348`; that is not GPU/owner acceptance. It does not implement the inspector or bloom,
prove reload/resize memory plateau, or replace final Windows/independent/owner
acceptance. The earlier Windows results remain valid only for `ef984ef`.

The production mesh path now copies up to 100 model/normal transforms per draw,
with at most 512 instances per batch. Empty instance spans retain the identity
case. Vertex and shadow entries use `SV_VulkanInstanceID`; seven finite shader
entries and the Vulkan 1.3/SPIR-V 1.5 capability baseline remain unchanged.
Paired, same-context geometry overrides let the scene schedule share the ship's
lighting and shadow pass with one genuinely instanced asteroid draw. Borrowed
CPU transforms are copied before return; submitted geometry retires by timeline.
Culled instances must share the requested facing, or use separate/both-sided draws.

`scene_workload` loads the private canonical package, renders LOD2, and adds 100
seeded instances of one original 80-triangle subdivided/deformed icosahedron.
The 600-frame orthographic camera replay encloses the ship and asteroid shell.
There are **238,080 ship + 8,000 asteroid = 246,080 triangles**, 189 draw records,
13 source materials, generated IBL and one 1024 shadow map. No new dependency was
added. The source model, package and captures remain private/ignored.

| Verification | Actual result |
|---|---|
| Clang Debug/Release/ASan and GCC scene graphs | 11/11 CTests each |
| Physical shaders/resources, each graph | Native instancing, normal, mixed-geometry, shadow, snapshot, retirement and validation controls pass |
| Full-package workload process checks | 4/4 each, including existing-output preservation and disabled GPU diagnostics |
| Existing full-package capture checks | 3/3 each after extracting the shared exclusive capture helper |
| Clang AST, exact native invocations | 69 analyses across Debug/Release/ASan, plus 19 shader-disabled analyses; no owned findings |
| Combined shader-disabled graph | 10/10 CTests |
| Asset-only / CPU-only regressions | 9/9 and 4/4 |
| PowerShell process/profile regression | Pass |
| Native ASan boundary controls | Forced single instancing and query-pool creation fail as intended; unsupported timestamps still render; wrapped timestamps produce exactly 32 ns |

The opt-in timer owns 16 timestamp queries in four proven reuse domains: two
resource timelines and two WSI submission fences. Results publish after successful
submission and are read only after acquired timeline completion. A 64-serial
result ring bounds retention. Queue valid bits and timestamp period govern masked
wrap arithmetic; unsupported or disabled diagnostics leave rendering available.
These TOP/BOTTOM stage intervals can overlap across passes: they are not summed
device-busy time, scanout, or presentation-retirement proof.

The final optimized Iris Xe run used 60 warmup frames followed by all 600 replay
frames at 1280×720 HDR, validation off, and no concurrent analysis run:

- Owner-loop latency: **16.677 ms P50, 17.985 ms P95, 18.684 ms P99; 19.451 ms max**.
- GPU interval P50/P95: shadow 0.720/1.215 ms, PBR 1.328/1.693 ms,
  post/FXAA 0.177/0.215 ms.
- Observed warm C++ `new`/`new[]`, including aligned forms: **zero**.
  This does not count C `malloc` or driver-private heaps. Validation-layer
  allocations remain included in validation runs; sanitizers keep their own
  allocator interposition (`allocation_probe=0`), not an equivalent zero claim.
- VMA allocation/reservation before capture: 210,844,808 / 214,396,216 bytes.
  These exclude WSI/opaque driver allocations and are not whole-GPU accounting.
- Linux `wait4` peak RSS, including cold loading/capture: 348,748 KiB.
  This is Linux residency, not Windows working set or graphics memory.

`measurement.json` records package/settings/source/capture identities. `frames.csv`
is the exact 600-row native report. The capture is identified, not committed.
The steady replay has no streaming reloads: its maximum is not a streaming-hitch
or reload/resize plateau result. Later UI/bloom changes require new measurements.

Reproduce from the repository root with a prepared `scene-release` graph:

```sh
cmake --build --preset scene-release
build/scene-release/scene_workload \
  build/cooked-candidate/frigate-canonical-first/scene.ngas \
  --frames=600 --warmup=60 --capture=build/new-flight.bmp --report=build/new-flight.csv
```

Use fresh output paths. `--validation` enables ordinary validation;
`--no-gpu-timings` exercises the diagnostic-disabled path. Explicit physical
process tests use `NGEN_WORKLOAD_EXE` and `NGEN_WORKLOAD_PACKAGE` with
`python tests/workload_cli.py`; they are not headless CTest.

Retained text normalizes the repository/Conan paths and trailing whitespace.
`originals.sha256` records original hashes, retained paths and source basenames;
`files.sha256` covers retained bytes. `sources.sha256` covers the relevant source
snapshot. `compile-commands.json` preserves native flags with path normalization;
results include original compilation-database hashes. GCC AST support is still
unavailable for its unmodified module flags; asset vendor advisories remain
outside this renderer/scene AST claim. See `review.txt` for repaired findings.
