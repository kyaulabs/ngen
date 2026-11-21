# Warmed two-system crossing probe

The physics-enabled `scene_probe --crossing-replay` warms up once and measures 20 crossings in one Vulkan context. Each crossing holds a missing-file request pending through warp, draws and reads back the proxy, rejects missing and corrupt packages, uploads a valid package, and admits arrival without changing the tick. After each crossing, tracked GPU resource records and allocation bytes must return to the settled baseline.

Build `scene_probe` with assets, renderer, shaders and physics enabled. Use the cooked public starter board from the [standalone project guide](../../creator-project.md), a nonexistent path, and a new file containing `not an NGAS package`. Enable `VK_LAYER_KHRONOS_validation` and run:

```sh
./build/demo-release/scene_probe --crossing-replay \
  /path/to/missing.ngas /path/to/corrupt.ngas /path/to/board/cooked/scene.ngas
```

Observed on Linux Clang 23.1.5, Intel Iris Xe (TGL GT2), Vulkan 1.4, driver `109064193`, Wayland with validation. The cooked package SHA-256 was `fab1e8b4072a47b86fd121b1485130ab9d13402a600fee61db6716449a13746f`.

| Measurement | Result |
|---|---:|
| Warmed crossings | 20 |
| Whole crossing p50 / p95 / max | 76.047 / 99.151 / 155.616 ms |
| Highest sampled allocation during destination upload | 6,756,024 bytes |
| Settled tracked allocation after each crossing | 6,755,652 bytes |

The timing includes file waits, proxy draw setup and synchronous readback, not startup and teardown. It is **not** a presented-frame or hitch measurement. Memory samples cover tracked allocations at upload poll points, not transient readback, driver/WSI memory or whole-process peaks. This does not close M4's flight-frame and memory acceptance gates.
