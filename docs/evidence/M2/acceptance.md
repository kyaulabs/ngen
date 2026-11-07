# M2 acceptance gates

**Open.** Implementation integration and automated fixtures are not independent graphics review or owner acceptance. The latest retained NVIDIA execution is MSVC/clang-cl **Debug at `e387dde`**, not this later IBL/shadow renderer or the full frigate.

| Requirement | Current evidence | Still required |
|---|---|---|
| M2-A: platform/capabilities | Intel/Wayland and owner Windows/RTX 5070 startup, surface selection, clear/resize/minimize checks | Final combined required-platform execution; scope recovery claims to exercised scenarios |
| M2-B: ownership/retirement | Tracked buffers/images, two pinned frigate versions, gated mesh/shadow resources, failed-submit controls | Whole-scene resize/reload memory plateau and final graphics-memory accounting, not just tiny-resource replacement or VMA counters |
| M2-C: offline shader contract | Seven finite validated/embedded entries; compiled reflection; native Intel material, IBL and shadow controls | New NVIDIA execution and compiler-choice acceptance; earlier five-entry Debug evidence remains revision-specific |
| M2-D: checked imported ship | Original source preserved; canonical package retains LOD0, all materials/lights and complete native mip/geometry readbacks | Full-package NVIDIA execution; redistribution remains unapproved |
| M2-E: fixed rendering schedule | Owning checked scene, generated IBL, indexed shadow → opaque/mask/sorted blend → post/WSI; target-resolution ship capture | 100-instanced-asteroid workload, restrained bloom, final material/scheduling/visual acceptance and optional UI composition |
| M2-F: queued inspector | Contract/planning only | Bounded next-frame selection/material/reload commands, inspector UI/walkthrough, disabled-tooling Release graph |

The laptop acceptance workload is **1280×720, one detailed ship plus 100 instanced asteroids, at most 250k visible triangles, one 1024 shadow map and at most 256 MiB texture working set**. Preserve its scene/settings hashes and fixed camera replay. After warmup, collect CPU/GPU pass and frame distributions, unexpected allocation counts and memory: median ≤50 ms, P95 ≤66.7 ms, P99 ≤100 ms, no streaming hitch >200 ms, GPU-owned allocations ≤1 GiB and process working set ≤6 GiB. Validation/sanitizer execution is separate from performance measurement.

The current cold orthographic ship capture selects 238,080 ship triangles but has **no asteroids, inspector, warmed replay or frame timing**. Its two-version VMA counters exclude WSI/driver memory. It cannot satisfy those budgets or establish displayed pixels.

Finish the remaining implementation and physical workload, resolve concrete findings, complete required-platform checks and the actual PR Test Plan, then obtain independent graphics review and owner visual acceptance. Keep the formal verdict open until those results exist. The [full-frigate Windows handoff](windows-frigate/README.md) closes only the execution scope it actually exercises.
