# M3 F12 performance overlay

The owner requests this before the broader UI refinement. F12 toggles a detailed
counter, hidden on startup; scene resets and camera/focus changes preserve its
visibility. Ignore key repeat and key-up. Use the existing ImGui/Context overlay,
not a new renderer pass or GPU owner.

Display FPS/frame time, individual completed GPU pass intervals, VMA allocation/
reservation and process/context-lifetime C++/ImGui allocation call counts. The
CLI's warm-window allocation counts remain separate. Label frame time as frame
time, not CPU execution time. GPU pass intervals may overlap and are not summed
GPU utilization or display latency.

The owner explicitly chooses **whole-system/device** CPU, RAM and VRAM usage:

- Aggregate system CPU busy percentage, based on successive OS counter samples.
- System RAM used/total; on Linux use total minus available, including reclaimable
  cache treatment in the OS's available estimate.
- Whole selected-adapter dedicated VRAM usage/total where the OS exposes it. Do
  not substitute process GPU usage, VMA allocation or Vulkan application budget
  estimates. On integrated/UMA hardware, label shared memory explicitly; report
  unsupported/stale metrics as unavailable rather than fabricated zeroes.

Use existing platform capabilities: Linux `/proc/stat` and `/proc/meminfo`, Windows
system CPU/memory APIs and read-only PDH adapter-memory counters matched to the
selected Vulkan adapter LUID. PDH is an OS SDK/library, not a new external package.
No WMI/CIM queries or subprocesses on the render thread. Copy adapter identity
before starting collection; the sampler owns no Vulkan handle or resource.

Collect low-rate OS readings on one bounded, cooperatively stopped worker, with
an owning copied snapshot and explicit synchronization. No periodic OS sampling
while F12-hidden or interactively minimized. Create/prewarm its worker/storage
and panel draw storage before the
constrained loop. Do not allocate per frame or grow existing overlay limits.
Sampling is not continuous peak/global benchmark proof; OS/provider failure must
not stop flight. Stop/join collection before frontend/Context teardown.

Implement public-behavior TDD for visibility, repeat rejection, counter deltas,
invalid/unavailable data, synchronization/shutdown and native F12 routing. Verify
Linux/Windows paths actually available, then owner-visible behavior and affected
allocation/performance checks.

The overlay is now implemented. [Native Linux verification](../evidence/M3/performance/README.md)
covers F12 routing/persistence, whole-system CPU/RAM, shared-memory labelling,
strict sanitizers, synchronized shutdown and allocation-free live-panel replays.
The Windows source cross-compiles, but native PDH/MSVC/ClangCl confirmation remains
required. This feature does not close M3's separate CIM/UI/budget gates.
