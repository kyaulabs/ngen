# Renderer, core and asset specification

Planning date: 2026-11-02. This is a proposed specification, not an accepted ADR or a validated dependency lock. No NGEN code, GPU, build, or benchmark was inspected or executed in this workstream. All performance numbers below are proposed acceptance targets. The `distill` skill and its pattern reference were read and applied.

## Recommended direction

Build the first flight demonstration around a small Vulkan 1.3 renderer, SDL3, VMA, EnTT, and an explicit simulation loop. Use forward rendering first, then clustered Forward+ when light counts justify it. Keep rendering downstream of immutable simulation snapshots. Neither the dedicated server nor offline simulation should link Vulkan, SDL video, asset importers, or online services.

Treat modern features as independently checked capabilities. Khronos distinguishes the instance/loader version from the physical-device version; a printed Vulkan 1.4 version is not proof of every desired feature, limit, texture format, or presentation path. The actual Iris Xe profile still needs inspection. [R1, R2]

## Module boundaries

These paths are proposed logical source areas. Reconcile them with the actual checkout before creating directories.

| Proposed module | Owns | Input/output contract | Forbidden dependency |
|---|---|---|---|
| `core` | IDs, units, clocks, errors, logging, bounded allocators | Value types and explicit ownership | SDL, Vulkan, ECS, game rules |
| `jobs` | Worker budget, task submission, cancellation and drain | Typed job handles; bounded task storage | Renderer or game objects |
| `platform/sdl` | Window, event pump, controllers and surface bridge | Timestamped input events; native surface handle | Authoritative game rules |
| `simulation` | Fixed-tick state and ordered system phases | Commands in; events and immutable snapshots out | Renderer, OS event loop, network socket |
| `assets/runtime` | Handles, residency, package validation | Content ID to versioned resident resource | Blender, glTF importer, credentials |
| `tools/assetc` | Import, validation, transform and package compilation | Source manifest to reproducible cooked package | Live game state |
| `render/vulkan` | Device, queues, GPU resources and frame execution | `RenderSnapshot`, camera and asset handles | Mutable authoritative ECS |
| `tools/inspector` | Development inspection and diagnostics | Read-only views; explicit queued edits | Direct hidden mutations of live state |

An offline executable assembles simulation, renderer, platform and local storage. A headless executable assembles simulation and whatever persistence/network adapters its game requires. CMake targets must enforce those boundaries; a comment is insufficient.

## Vulkan capability contract

The minimum renderer requests Vulkan 1.3. Query the selected physical device with `vkGetPhysicalDeviceProperties2` and `vkGetPhysicalDeviceFeatures2`; record UUID, driver ID/version, API version, queue families, feature bits, limits, formats, memory heaps and surface capabilities. Explicitly enable the used features. Khronos documents this as the application's responsibility. [R1, R2]

Require graphics/present support, suitable color/depth attachments, `dynamicRendering`, `synchronization2` and `timelineSemaphore`. Start with ordinary per-material descriptors and indexed drawing. Buffer device address, bindless descriptor indexing, mesh shaders, ray tracing, shader FP64, descriptor buffers and Vulkan 1.4-only facilities are not prerequisites for first light.

Optional descriptor-indexing mode must test the exact bits it consumes, including sampled-image nonuniform indexing, runtime arrays, partially bound descriptors and update-after-bind only when used. Check descriptor counts and update-after-bind limits separately. Likewise, an indirect-rendering path needs its own supported-feature and limit contract. An optional feature mask should allow CI to force the basic path on a capable GPU.

The fallback policy is a maintained Vulkan 1.3 basic renderer. A device below that baseline receives an actionable compatibility error. A Vulkan 1.2 extension path should be a separately justified support decision; adding it now would double initialization and test cases without an identified target.

SDL provides required instance extensions, function lookup and surface creation, while Vulkan owns presentation. Test native Wayland, resize, minimization, zero drawable extent, scale changes, focus changes, controller hotplug and swapchain invalidation. [R4]

## Renderer specification

M2 uses a fixed pass sequence: shadow depth, opaque PBR, transparent objects/particles, post-processing and UI. Keep an explicit resource-use table for each pass. No transient aliasing or asynchronous compute initially. Add a formal graph only when passes or quality paths make manual synchronization error-prone. AMD RPS is an available Vulkan/D3D12 graph framework with an MIT license in its current repository; its older launch article mentions an evaluation license. Evaluate the version actually selected. A bespoke general graph requires an ADR comparing integration cost and measured scheduling/CPU costs. [R20]

Choose a metallic-roughness material baseline with linear lighting, correct sRGB decode/encode, normal maps, image-based lighting, one directional key light, local lights, shadows, exposure, tone mapping and restrained bloom. glTF 2.0 specifies metallic-roughness materials, right-handed coordinates, meters and radians. Preserve that import contract or document a single conversion at the cooker boundary. [R21]

Forward+ is the provisional destination. It preserves one opaque material path and a usable transparent path while avoiding an early large G-buffer. This does not establish that it is faster than deferred rendering on NGEN scenes. Compare representative many-light workloads before changing renderer family. Start with capped simple forward lighting until that comparison is needed.

Use float camera-relative GPU transforms. Subtract the camera origin in the CPU coordinate representation before narrowing. Keep the current and previous transform in the same reference frame for motion vectors; origin changes must not appear as object velocity. Use reversed-Z with a floating depth format when supported, and validate near/far behavior. Reversed-Z improves depth-buffer precision; it does not fix imprecise world-space transforms. [R22]

Render very distant celestial bodies through a separate scale-aware sky/celestial path with explicit occlusion composition. Large mesh vertices must remain near a local origin. Do not upload galaxy coordinates as floats or require GPU doubles. Test shadow maps, culling, particles and temporal histories during origin changes and warp, not only the primary ship mesh.

Begin with one graphics queue and two frame contexts. Each context owns command pools, transient descriptors and bounded upload storage. An ordinary submitted GPU resource's C++ destructor queues retirement; actual destruction waits for its last using submission. Presentation resources have a separate retirement contract. Idle waits are acceptable during controlled shutdown/recovery, not every frame. Maintain separate ownership for swapchain images. Buffer present-wait semaphores by acquired image index and reuse them only after the matching acquisition wait; a frame-submit fence alone does not prove presentation is finished. Prefer `VK_KHR_swapchain_maintenance1` or its EXT equivalent with present fences for recreation/shutdown retirement. Without that capability, idle-based teardown is a documented prototype limitation, not a specification proof; production lifetime acceptance remains open until a justified retirement path is selected. Test resize while uploads are pending, resource replacement, partial initialization failure and shutdown. [R23, R24]

GPU-driven culling, indirect dispatch, ray tracing, volumetrics and vendor reconstruction belong to later measured work. Motion vectors, exposure history, jitter and disocclusion information should have stable definitions early so temporal reconstruction does not require changing simulation or asset formats later.

## Shader pipeline

Compile shaders offline to SPIR-V with an explicit target environment and feature profile. Store compiler executable and dependent-library hashes. Validate SPIR-V against the selected Vulkan environment; generate reflection metadata for bindings, push constants and buffer layouts. Check CPU layout assumptions with static assertions and shader interface tests.

Slang's official documentation warns that mixed installations can load the wrong shared libraries. Ship the compiler only with tools and isolate its search path. Build-time shader compilation uses a build-profile tool dependency, not a library linked into the game. [R12]

The compiler choice gate uses the same PBR vertex/fragment pair and a compute culling sample on Intel/Linux and NVIDIA/Windows. Check descriptor layout, matrix conventions, SPIR-V validation, compile time and rendered output. Keep specialization/permutation counts finite and report cache misses. GLSL/glslang is a complete alternative if this gate fails; it should not become a permanent second production pipeline without a concrete need.

Offline SPIR-V compilation does not remove driver pipeline creation work. Prepare known M2/M3 pipelines before interactive play; perform streamed pipeline creation through a bounded preparation path outside the critical frame loop. Publish an asset/material only when its pipeline and upload are ready, retaining a prepared proxy/basic material meanwhile. A compatible persistent `VkPipelineCache` may reduce work but does not guarantee zero stutter. Validate cache identity/compatibility and record cold-cache startup/streaming separately from warmed frame tests. This requires a small preparation queue, not a universal pipeline framework. [Khronos pipeline cache guidance](https://docs.vulkan.org/samples/latest/samples/performance/pipeline_cache/README.html)

## Memory, ECS and scheduling

Use `unique_ptr` or owner members for long-lived ownership, spans/references for borrowed access and generation-checked handles for entities/resources. Keep persistent game IDs distinct from transient ECS entity numbers. Avoid retaining component pointers across structural changes. EnTT documents sparse-set storage and leaves scheduling to the caller; Flecs supplies broader relationships and tooling. Neither proves the better performance choice without NGEN data. [R8, R9]

Reserve capacities at scene/region load. Track per-subsystem bytes, high-water marks and allocation counts. Frame scratch arenas reset only when their consumers finish. Give worker tasks bounded scratch storage. Streaming can allocate within a budget; fixed-tick gameplay and frame preparation should have measured zero unexpected heap allocation after warmup. Capacity exhaustion must produce a defined error, eviction, admission decision or backpressure, not silent buffer growth.

Use VMA for device memory and query budgets when supported. An integrated GPU shares system RAM; advertised heap size is not an allocation allowance, and CPU RSS plus GPU allocation counts can double-count shared pages. Set independent subsystem caps plus measured total process/system pressure. Keep a fallback placeholder asset resident. Avoid defragmenting in-flight resources until relocation handling is designed and tested. [R6]

Use an explicit phase graph: accept commands, flight/AI, physics, resolve interactions, emit events, extract presentation state. Structural ECS changes occur at defined barriers. Parallel jobs declare read/write sets or receive disjoint ranges. Merge results in deterministic order where authoritative state requires it. Scheduling determinism alone does not make floating-point physics deterministic across platforms.

enkiTS is the provisional runtime scheduler; Taskflow is the comparison candidate. Use one coordinated CPU worker budget. Do not give enkiTS, physics, streaming and shader compilation independent full-machine pools. Blocking disk/database/network work uses separate bounded execution, never a task that blocks every simulation worker. Start with a serial reference path. Test cancellation, shutdown, nested dependencies, queue saturation and task lifetime; use ThreadSanitizer on CPU-only scenarios.

## Asset contract

Keep editable source assets and provenance separate from cooked runtime packages. Each source manifest includes content ID, source hashes, license/provenance, import settings, coordinate/unit convention, compiler version and dependency hashes. Cooker output includes schema version, bounds, LODs, collision references, textures/materials, dependencies and checksums. Runtime handles include a generation/version to detect stale references. Each snapshot/job also holds a pin or lease on the resolved resident version; keep it alive through CPU consumption and last GPU use. Publish replacement atomically and retire the old version only after all pins and GPU work finish. Test replacement/eviction during concurrent extraction and pending uploads.

The initial importer supports a declared glTF 2.0 subset through fastgltf. Reject unsupported required extensions with a useful diagnostic. Validate NaNs, vertex/index bounds, malformed accessors, path traversal, decompression sizes and missing textures. Importer tests run on both valid fixtures and a fuzz corpus. Generated 3D assets receive the same checks as human-authored assets.

Use meshoptimizer for optimization/LOD preparation, then validate silhouette and collision behavior. Use KTX2 with an explicit texture/color-space policy and hardware-supported native formats. Decode/transcode in background jobs, stage uploads, and publish residency only after GPU completion. Prefetch from committed warp routes. The fallback is visible proxy geometry/materials plus paced arrival, not a synchronous multi-second load on the render thread.

KTX's 4.4.2 release notes report cross-platform differences in Basis output and multithreaded UASTC RDO nondeterminism. Therefore the reproducibility promise is a pinned canonical cooker environment and settings, with resulting content hashes, not universal bit identity across arbitrary Windows/Linux encoders. [R16]

## Proposed measurable gates

| Gate | Fixed workload and evidence | Proposed pass condition |
|---|---|---|
| Device/WSI | Original Iris Xe JSON plus fresh capture; 1,000 resize/minimize cycles and 30-minute smoke | Correct selected GPU; required bits pass; no validation errors, leaks or crash. Record unsupported optional tiers. |
| Laptop flight | 1280×720, low preset, one detailed ship, 100 instanced asteroids, ≤250k visible triangles, one 1024 shadow map, no RT, ≤256 MiB texture working set | After warmup: median ≤50 ms; P95 ≤66.7 ms; P99 ≤100 ms; no streaming hitch >200 ms. GPU-owned allocations ≤1 GiB and process working set ≤6 GiB as provisional caps. |
| Desktop flight | Same scene/content hash at 1920×1080, medium preset; GTX 1660 Super only after passthrough verified | Initial target P95 ≤33.3 ms. RTX 5070 high-preset target P95 ≤16.7 ms. Recalibrate only through an explicit evidence-backed ADR. |
| Precision | Same camera/ship formation near origin, at AU separation and after interstellar travel; repeated rebases | No visible transform/velocity spike; ≤0.25-pixel projected difference in comparison fixtures; no invalid depth/shadow values. |
| ECS/jobs | Representative ship components and projectile churn; serial, 1/2/4/8-worker runs | Same semantic outputs; bounded allocation; no races. Compare ECS alternatives only if the chosen path threatens its agreed tick budget. |
| Assets | Repeated cook in canonical environment; corrupt/missing/cancelled streams; capacity exhaustion | Equal hashes where promised; explicit failure; no unbounded growth, deadlock or use-after-free. |
| GPU lifetime | Repeated load/unload and resize with in-flight work | No synchronization/object-lifetime validation error and bounded memory plateau. |

These are proposed budgets, not measured capability claims. Record build SHA, dependency lock, scene hash, settings, resolution, driver, power state, temperatures/clocks, median/P95/P99, peak memory and profiler trace. Use at least three warmed runs with a fixed camera replay. Validation builds and performance builds produce separate evidence. Software Vulkan is useful for a smoke test but does not count as passing a physical-GPU performance gate. The laptop gate does not cover the 200-ship AI battle.

## Renderer experiments

1. **P0, capability capture:** collect the three machines' physical-device facts without installing services. Resolve any missing baseline feature before renderer design hardens.
2. **P0, WSI/lifetime spike:** surface, swapchain, validation and retirement behavior under resize/cancel/shutdown. This prevents every later visual milestone inheriting unsafe resource ownership.
3. **P1, shader/layout spike:** one PBR pair and compute kernel, one pinned compiler, Intel/NVIDIA output comparison. Decide Slang versus GLSL once.
4. **P1, precision spike:** camera-relative meshes, shadows and temporal history across astronomical origins. It must precede polished warp effects.
5. **P1, representative simulation benchmark:** EnTT/enkiTS baseline with physics worker coordination; compare Flecs/Taskflow only against concrete costs.
6. **P1, asset pressure spike:** canonical cook, low-memory streaming, cancellation, repeated replacement and malformed data.
7. **P2, graph/upscaler experiment:** only after the existing renderer has captured CPU/GPU bottlenecks. Compare a proven graph library before authorizing a bespoke graph; benchmark reconstruction on the actual supported GPUs.

M1 should establish core types, ownership/error conventions, bounded buffers, profiling and test targets. M2 should add platform/WSI, GPU ownership, shader cooking and a single imported PBR ship in separate reviewable changes. M3 adds assisted flight, camera, restrained effects, audio and a fixed replay that proves the beautiful-flight gate. Keep each change independently testable and make performance claims only from attached traces.

## Primary sources and retrieval references

- R1: Khronos, Vulkan versions and instance/device distinction: https://docs.vulkan.org/guide/latest/versions.html.
- R2: Khronos, feature/limit/format queries: https://docs.vulkan.org/guide/latest/querying_extensions_features.html; release promotion summary: https://docs.vulkan.org/guide/latest/vulkan_release_summary.html.
- R3: SDL release assets: https://github.com/libsdl-org/SDL/releases.html; Conan SDL3 3.4.16 and Wayland options: https://conan.io/center/recipes/sdl?version=3.4.16.
- R4: SDL Vulkan integration: https://wiki.libsdl.org/SDL3/CategoryVulkan.
- R5: VMA release/changelog: https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/releases; https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/blob/master/CHANGELOG.md.
- R6: VMA docs: https://gpuopen-librariesandsdks.github.io/VulkanMemoryAllocator/html/; Conan: https://conan.io/center/recipes/vulkan-memory-allocator.
- R7: EnTT 4.0 release: https://github.com/skypjack/entt/releases/tag/v4.0.0; Conan snapshot: https://conan.io/center/recipes/entt.
- R8: EnTT design/ownership: https://skypjack.github.io/entt/md_docs_2md_2entity.html; license: https://github.com/skypjack/entt.
- R9: Flecs design: https://www.flecs.dev/flecs/DesignWithFlecs.html; releases: https://github.com/SanderMertens/flecs/releases; Conan: https://conan.io/center/recipes/flecs; license: https://github.com/SanderMertens/flecs.
- R10: enkiTS: https://github.com/dougbinks/enkiTS; Conan 1.12: https://conan.io/center/recipes/enkits.
- R11: Taskflow: https://github.com/taskflow/taskflow; Conan: https://conan.io/center/recipes/taskflow.
- R12: Slang release assets: https://github.com/shader-slang/slang/releases; setup/library loading: https://docs.shader-slang.org/en/latest/external/slang/docs/user-guide/01-get-started.html; license: https://github.com/shader-slang/slang/blob/master/LICENSE.
- R13: GLSL compiler: https://github.com/KhronosGroup/glslang; DXC SPIR-V: https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst.
- R14: fastgltf releases: https://github.com/spnda/fastgltf/releases; Conan: https://conan.io/center/recipes/fastgltf; cgltf alternative: https://conan.io/center/recipes/cgltf.
- R15: meshoptimizer releases: https://github.com/zeux/meshoptimizer/releases; Conan: https://conan.io/center/recipes/meshoptimizer.
- R16: KTX release/reproducibility caveats: https://github.com/KhronosGroup/KTX-Software/releases/tag/v4.4.2; licensing: https://github.com/KhronosGroup/KTX-Software/blob/main/LICENSE.md; Conan: https://conan.io/center/recipes/ktx.
- R17: Tracy features: https://github.com/wolfpld/tracy; release: https://github.com/wolfpld/tracy/releases/tag/v0.14.1; Conan: https://conan.io/center/recipes/tracy; license: https://github.com/wolfpld/tracy/blob/master/LICENSE.
- R18: Catch2 Conan snapshot: https://conan.io/center/recipes/catch2. Resolve upstream tag/date before locking.
- R19: Google Benchmark Conan: https://conan.io/center/recipes/benchmark.
- R20: AMD RPS current repository/license: https://github.com/GPUOpen-LibrariesAndSDKs/RenderPipelineShaders; vendor description: https://gpuopen.com/rps/.
- R21: Khronos glTF 2.0 specification: https://github.com/KhronosGroup/glTF/blob/main/specification/2.0/Specification.adoc.
- R22: NVIDIA, depth precision/reversed Z: https://developer.nvidia.com/blog/?p=32034. Vendor explanation, not a NGEN benchmark.

- R23: [Khronos presentation semaphore reuse](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html).
- R24: [Presentation resource retirement fences](https://docs.vulkan.org/refpages/latest/refpages/source/VkSwapchainPresentFenceInfoKHR.html).
