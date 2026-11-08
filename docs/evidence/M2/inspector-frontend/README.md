# Tracked inspector frontend

The development inspector now runs over the ship/100-asteroid workload. This
completes the local frontend/reload integration, **not M2 acceptance**. Bloom,
final material/visual quality, combined performance/whole-memory and plateau
measurements, changed-tree Windows/NVIDIA execution and independent/owner
acceptance remain open. The completed `ef984ef` Windows slice needs no repeat.

The original dark, compact cyan/amber design follows the
[static study](../../../mockups/README.md) without copying EVE assets or fonts.
Dear ImGui **1.92.9b** is the available pinned Conan package; the proposed 1.93.0
recipe was unavailable. `conan-inspector.lock` adds only ImGui, preserving every
existing scene-lock reference/timestamp. Its packaged SDL platform backend uses
NGEN's locked SDL. MIT/font notices are in `licenses/imgui/`; NGEN remains AGPL.
There is no ImGui Vulkan backend, independent GPU queue/resource owner, runtime
shader compiler, external font, or ImGui settings/log file.

The Context-owned overlay follows tone mapping/FXAA. Its fixed profile is 8,192
vertices, 16,384 uint32 indices and 128 clip commands. Four completion domains
copy geometry into tracked upload buffers; atlas/buffer last-use publishes only
on successful submission. Packed sRGB vertex RGB is decoded before linear alpha
blending; font alpha remains linear. Separate post/UI timestamps are acquired
only after graphics completion. They can overlap other stages and are not summed
GPU busy time or displayed-pixel proof.

Controls enqueue generation-bound selection, material factors and exposure/FXAA
for the next boundary. Selection bounds and diagnostics use owning snapshots.
Reload reads one checked package on the dedicated cancellable worker, then
advances tracked GPU preparation on the owner thread. Only a completed GPU
version stages model publication. Old CPU/GPU frame pins remain valid; failed
read/admission/upload retains the current version and reports failure. Blocking
I/O and third-party processing are not hard-deadline interruptible or sandboxed.
The frontend must be destroyed before Context closes its borrowed SDL window.

| Actual local verification | Result |
|---|---|
| Clang 23 Debug/Release/ASan+UBSan and GCC 16 Debug | 12/12 CTests in each graph |
| Physical inspector/material/overlay probe | Four graphs; analytic sRGB/alpha/coverage/clipping, malformed/capacity/foreign input, copied pending geometry, atlas retirement, queued WSI and actual frontend/resize-race controls |
| Native UI process suite | 3/3 per graph: composited capture, warmed allocation controls, 600-frame edit/reload/resize walkthrough |
| Separate headless model TSan | 6/6; no GUI or GPU renderer targets |
| Inspector-disabled Release | 11/11; no inspector/ImGui/overlay-pipeline TUs, targets or linked implementation symbols; `--inspector` rejects before native work |
| Shader-disabled graph | 10/10; current affected consumers built/analyzed |
| Actual Clang AST | 111 affected native invocations across five configurations; required repair reruns complete that scope |
| Native injected failures under ASan/UBSan/leaks | Eight overlay layout/pool/set/sampler/module/pipeline/buffer/submit failures; ordinary validation remains enabled |

The workflow uses public model commands for selection/material/view edits, then
an SDL keyboard event through the actual ImGui backend for reload. It verifies
next-boundary edits, generation 2 after completed replacement, continued drawing,
and actual 1024x640 then 1280x720 swapchain extents/two new generations. This is
an automated integration walkthrough, not personal mouse/visual acceptance.

A separate validation-enabled 600-frame/60-warmup trace and exact CSV are retained.
Its private BMP is 3,686,538 bytes; its hash is in `capture.json`, not its pixels.
That run observed 9,933,389 process C++ allocation calls (including validation and
reload work) and three ImGui calls during reload/resize. It ran alongside AST
analysis: **not performance acceptance or a zero-allocation streaming claim**.
The separate warmed, validation-off UI test observes zero C++ `new` calls where
instrumentation is enabled and zero ImGui allocation calls. These counters do
not cover SDL/C `malloc`, opaque driver heaps, whole-GPU memory or working set.
The HUD labels owner-loop wall time `FRAME`, not CPU busy time.

Retained failures include the first missing-overlay pixel Red and the genuine
resize race: the drawable shrank after CPU layout, causing target-bound admission
to reject valid pending UI. WSI now preserves bounded profile admission and
intersects scissors with the acquired target; offscreen post admission remains
strict. Initial ASan executables could not start under this host's PIE mapping;
the existing process/build-local `-no-pie` layout fixes startup without removing
instrumentation. An initial 180-frame ASan walkthrough ended while CPU parsing
was still active; the unchanged 120-second process deadline and canonical
600-frame replay now verify completion. Neither setup/test-budget failure is a
behavioral Red. AST findings and their repairs are retained without suppression.

```sh
sh tools/prepare.sh inspector-debug
cmake --preset inspector-debug
cmake --build --preset inspector-debug
ctest --preset inspector-debug
# Explicit physical checks with the ordinary validation layer available:
build/inspector-debug/inspector_probe
build/inspector-debug/scene_workload scene.ngas --inspector --walkthrough \
  --frames=600 --warmup=60 --validation --capture=new.bmp --report=new.csv
```

Use `inspector-{release,asan,gcc}` for the other local graphs. On this hardened
host the ASan configure additionally uses
`-DCMAKE_EXE_LINKER_FLAGS="-m64 -fsanitize=address,undefined -no-pie"`.
`NGEN_BUILD_INSPECTOR=OFF` remains the shipping default. The assets-only graph
still supports the headless model through `NGEN_BUILD_INSPECTOR=ON`; it needs no
ImGui when renderer/shaders are absent. Offline contract tests validate nine
shader entries; unused overlay blobs are not linked into the shipping workload.

`files.sha256` covers retained evidence; `originals.sha256` records original
inputs before text-path/whitespace normalization. `sources.sha256`, native commands and
invocation-specific compile metadata identify the tested tree. GCC's known AST
flag limitation and existing asset-vendor advisories remain separate. Review is
same-session, not independent graphics review.
