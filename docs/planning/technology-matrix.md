# Technology decision matrix

Research date: 2026-11-02. Every selection below is a proposal. Exact versions are adoption candidates verified in primary release or package records, not a successfully resolved NGEN lockfile. Preserve existing working choices until the private audit establishes a reason to replace them. Package availability, upstream release availability and compatible binaries are separate facts.

## Engine and tools

| Area | Proposed candidate | Alternative / reason for choice | Maintenance, compatibility and adoption gate |
|---|---|---|---|
| Build | CMake 4.4.5 + Ninja 1.13.2 | Meson is capable, but CMake is required and fits the chosen libraries | Pin both. CMake 4 can expose old dependency policy assumptions; exercise every recipe. [T1] |
| Dependencies | Conan 2.34.0, `CMakeToolchain` + `CMakeDeps` | Keep experimental `CMakeConfigDeps` out of the default until a concrete need is proven | Host/build profiles, recipe revisions, package IDs, strict multi-profile locks. No assumption that a lock makes all binaries reproducible. [T2] |
| Compiler | LLVM/Clang 23.1.4 candidate | GCC and MSVC compatibility lanes remain mandatory | Record standard library and Windows CRT/ABI. Confirm Conan recipes understand compiler version. Toolchain licenses/notices include LLVM exception. [T3] |
| Math | Reuse existing math first; GLM 1.0.3 if absent | Do not write a general vector/matrix library | GLM is MIT-compatible dual licensed; retain notices. Put strong SI unit and coordinate types around math values, keep galaxy representation separate. Conan recipe/build still to verify. [T4] |
| Window/input/audio | SDL3 3.4.18 | GLFW is narrower; SDL covers controllers/audio and both target OSes | zlib license. Release dated 2026-10-02; retrieved Conan page lists 3.4.16. Verify native Wayland build options and selected driver. [T5] |
| Graphics API | Vulkan 1.3 capability baseline; 1.4 optional | No second rendering API for current scope | Pin one compatible SDK/header/tool set at M0; runtime features/limits/WSI decide compatibility. No GPU FP64 or RT requirement. [T6] |
| GPU memory | VMA 3.4.0 | Bespoke allocator is unjustified | MIT; release dated 2026-06-05; retrieved Conan page 3.3.0. Use a vetted pinned recipe if needed. Generated 3.5 docs are not a released version. [T7] |
| ECS | EnTT 4.0.0 candidate | Flecs 4.1.6 offers relationships/prefabs/integrated scheduling; select only if its workload/tooling benefit is demonstrated | MIT both. EnTT 4 requires C++20/CMake 3.28, has major API changes; retain an existing 3.x version unless migration pays for itself. Conan 4.0 packaging unverified. [T8] |
| Jobs | enkiTS 1.12 candidate | Taskflow 4.0.0 for more explicit dependency-graph workflows | zlib vs MIT. Conan enkiTS recipe observed. Single coordinated worker budget; benchmark task granularity, cancellation and contention. No bespoke scheduler. [T9] |
| Physics | Jolt 5.6.0, double-position CPU build | PhysX/Bullet remain comparison candidates if scale/contact experiment fails | MIT. Version-specific Conan 5.6.0 page exists. Disable optional GPU interfaces and verify headless linkage. Capital hulls/relative velocities need tests. [T10] |
| Shaders | Slang 2026.20 offline to SPIR-V | GLSL/glslang is the complete fallback; HLSL/DXC only for a concrete integration need | Apache-2.0 WITH LLVM-exception plus bundled notices. Release dated 2026-10-17. Pin compiler and shared libraries as tools; Conan packaging to verify. One production dialect. [T11] |
| Renderer organization | Fixed forward/PBR pass schedule, then clustered Forward+ | Deferred/hybrid only after scene-specific light/overdraw/bandwidth comparison | This is an architectural choice, not a performance claim. Keep transparent rendering and low-memory tier viable. No universal graph in M2. |
| Temporal reconstruction | Optional DLSS/DLAA and FSR comparison at M11 | Native rendering/TAA remains the cross-vendor fallback | Current candidate evidence: DLSS 4.5 suite, Streamline 2.14.16; AMD SDK 2.4 with FSR 3.1.6/ML 4.1.2. Vulkan API support does not prove Linux binary support. Exact terms and capability gates are in [advanced rendering](advanced-rendering.md). |
| Advanced GI/volumetrics | Probe/RT experiments and bounded nebula rendering at M11 | Authored/baked lighting and non-RT approximations for basic tier | Evaluate RTXGI 2.x components where their Vulkan/OS/license requirements fit. Current AMD Ray Regeneration/Radiance Caching are not assumed Vulkan features. See [comparison and sources](advanced-rendering.md). |
| Render graph | Evaluate AMD RPS when explicit pass management becomes costly | Small domain-specific scheduler only if measured integration tradeoff favors it | Current repository MIT; old evaluation-license article is not controlling. Verify exact revision, compile integration and barriers. Deferred from first light. [T12] |
| Import | fastgltf 0.9.1, tools only | cgltf 1.15 if the required subset favors a small C parser | MIT. Release/package page agree. Validate unsupported glTF extensions and malformed inputs; do not ship source importer in runtime. [T13] |
| Geometry | meshoptimizer 1.0 recipe candidate; evaluate upstream 1.3 fixes | Avoid bespoke LOD/compression until an actual gap appears | MIT. Conan recipe observed without listed prebuilt binaries; source build is valid. Lock after image/mesh quality validation. [T14] |
| Textures | KTX-Software 4.4.2 / KTX2, hardware-checked BC formats | Keep uncompressed/proxy fallback for unsupported formats and failures | Mainly Apache-2.0 plus bundled licenses. Canonical cooker required because encoder output can vary across platform/thread settings. [T15] |
| Inspector | Dear ImGui 1.93.0 candidate | Full retained-mode editor framework is premature | MIT. Development-only inspector with queued validated edits; no hidden authoritative writes. Release verified 2026-10-14; Conan integration pending. [T16] |
| Profiling/debugging | Tracy 0.14.1 + RenderDoc 1.47 | GPU vendor tools supplement, not replace, cross-vendor captures | BSD-3-Clause and MIT respectively. Releases dated 2026-08-22/2026-10-31. Tracy Conan page observed. Disable remote profiling in shipped products; verify capture support on each driver. [T17] |
| Tests/benchmarks | Catch2 3.16.0 + Google Benchmark 1.9.5 candidates | Existing project framework wins if adequate | BSL-1.0 and Apache-2.0. Catch2 v3 is compiled, not header-only. Meaningful property/fault tests and scenario benches remain NGEN work. [T18] |

## Multiplayer and persistence

| Area | Proposed candidate | Alternative / reason for choice | Integration and operational gate |
|---|---|---|---|
| Transport | GameNetworkingSockets 1.6.1 | MsQuic if a measured transport problem warrants a different API | BSD-3-Clause. Release verified 2026-10-03; retrieved generic Conan page lists 1.4.1. Use an audited 1.6.1 recipe, not a silent downgrade. GNS does not provide accounts, replication or automatic Steam relay entitlement. [T19] |
| Replication | NGEN-owned versioned input/snapshot schema over transport | Whole-world deterministic lockstep rejected for this latency/physics scope | Bound sizes and rates; predict controlled-flight model only; test stale baselines, loss, replay and control transfer. These are game semantics, not a transport-library feature. |
| Database | PostgreSQL 18.6 | Local atomic saves for offline M5; no distributed SQL system initially | PostgreSQL license. Official release date 2026-08-13; 18.5 was skipped. Verify image digest, durability settings, backup/PITR and concurrency semantics. [T20] |
| SQL client | libpqxx 8.0.2 + compatible libpq (17.11 recipe observed) | Direct libpq if measured async I/O requirements justify complexity | BSD-style libpqxx and PostgreSQL client license. C++20 packages observed; client/server version compatibility must be tested. Exceptions contained at persistence worker. [T21] |
| Scripting | Lua 5.5.1, first-party scripts only | Luau if typing/tooling creates a measured authoring advantage | MIT. Lua release dated 2026-08-03; Conan 5.5.0 observed, 5.5.1 recipe pending. Bytecode/version lock and C++ RAII error-boundary test required. No modding or arbitrary user code. [T22] |
| Distributed ownership | Single-writer regions and durable fencing epochs, introduced at M10 | Start with one process and many logical regions | No Kubernetes/service mesh dependency by default. Choose a coordinator only from tested failover requirements; a lease alone is insufficient. |
| Blockchain | Internal journal first; optional external settlement service | Provider custody vs self-custody evaluated at M12 | No chain chosen today. If EVM wins, evaluate pinned audited OpenZeppelin 5.x contracts; network, token, custody, compliance and cost remain a separate go/no-go. [T23] |
| Art | Dated GPT Image 2.5 models; Blender-authoritative asset workflow | Meshy 7.1, TRELLIS.2 and TripoSR comparison, plus manual baseline | Development tools only. Exact rates, licensing/VRAM limits and experimental 4K caveat are in `art-pipeline.md`. No generation was performed. |

## Why these recommendations remain provisional

Recent releases show active maintenance, not immunity from regressions. Version pins minimize drift, not security risk. Review upstream release notes and known security advisories when creating a lock; rerun affected compatibility tests on upgrades. An official Conan page proves a recipe/package record exists, not that NGEN's architecture, build type, options or sanitizer profile has a working binary.

Do not run synthetic ECS benchmarks and declare an engine-wide winner. Compare representative ship/module iteration, spawn/despawn churn, projectiles and snapshot extraction. Do not choose deferred rendering from desktop results alone; measure the integrated GPU's bandwidth and memory. Do not select a transport from loopback throughput alone; use loss/jitter/MTU and per-client budgets. Keep final licenses, source hashes, build options and accepted evidence in a dependency manifest.

## Sources

- T1: [CMake 4.4.5](https://github.com/Kitware/CMake/releases/tag/v4.4.5), [Ninja 1.13.2](https://github.com/ninja-build/ninja/releases/tag/v1.13.2).
- T2: [Conan 2.34.0](https://pypi.org/project/conan/2.34.0/), [lockfiles](https://docs.conan.io/2/tutorial/versioning/lockfiles.html), [experimental generator](https://docs.conan.io/2/reference/tools/cmake/cmakeconfigdeps.html).
- T3: [LLVM 23.1.4](https://github.com/llvm/llvm-project/releases/tag/llvmorg-23.1.4).
- T4: [GLM 1.0.3](https://github.com/g-truc/glm/releases/tag/1.0.3), [GLM source/license](https://github.com/g-truc/glm).
- T5: [SDL releases](https://github.com/libsdl-org/SDL/releases), [SDL Vulkan interface](https://wiki.libsdl.org/SDL3/CategoryVulkan), [Conan SDL](https://conan.io/center/recipes/sdl).
- T6: [Khronos versions](https://docs.vulkan.org/guide/latest/versions.html), [feature enabling](https://docs.vulkan.org/guide/latest/enabling_features.html).
- T7: [VMA releases](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/releases), [changelog](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator/blob/master/CHANGELOG.md), [Conan VMA](https://conan.io/center/recipes/vulkan-memory-allocator).
- T8: [EnTT 4.0.0](https://github.com/skypjack/entt/releases/tag/v4.0.0), [Flecs](https://github.com/SanderMertens/flecs), [Conan EnTT](https://conan.io/center/recipes/entt).
- T9: [enkiTS](https://github.com/dougbinks/enkiTS), [Conan enkiTS](https://conan.io/center/recipes/enkits), [Taskflow](https://github.com/taskflow/taskflow).
- T10: [Jolt 5.6.0](https://github.com/jrouwe/JoltPhysics/releases/tag/v5.6.0), [archived architecture](https://jrouwe.github.io/JoltPhysicsDocs/5.6.0/), [Conan 5.6.0](https://conan.io/center/recipes/joltphysics?version=5.6.0).
- T11: [Slang releases](https://github.com/shader-slang/slang/releases), [Slang license](https://github.com/shader-slang/slang/blob/master/LICENSE), [glslang](https://github.com/KhronosGroup/glslang), [DXC SPIR-V](https://github.com/microsoft/DirectXShaderCompiler/blob/main/docs/SPIR-V.rst).
- T12: [AMD RPS](https://github.com/GPUOpen-LibrariesAndSDKs/RenderPipelineShaders).
- T13: [fastgltf releases](https://github.com/spnda/fastgltf/releases), [Conan fastgltf](https://conan.io/center/recipes/fastgltf), [cgltf](https://github.com/jkuhlmann/cgltf).
- T14: [meshoptimizer releases](https://github.com/zeux/meshoptimizer/releases), [Conan recipe](https://conan.io/center/recipes/meshoptimizer).
- T15: [KTX 4.4.2](https://github.com/KhronosGroup/KTX-Software/releases/tag/v4.4.2), [licenses](https://github.com/KhronosGroup/KTX-Software/blob/main/LICENSE.md).
- T16: [Dear ImGui 1.93.0](https://github.com/ocornut/imgui/releases/tag/v1.93.0).
- T17: [Tracy 0.14.1](https://github.com/wolfpld/tracy/releases/tag/v0.14.1), [Conan Tracy](https://conan.io/center/recipes/tracy), [RenderDoc 1.47](https://github.com/baldurk/renderdoc/releases/tag/v1.47).
- T18: [Catch2 3.16.0](https://github.com/catchorg/Catch2/releases/tag/v3.16.0), [Conan Catch2](https://conan.io/center/recipes/catch2), [Conan Google Benchmark](https://conan.io/center/recipes/benchmark).
- T19: [GNS 1.6.1](https://github.com/ValveSoftware/GameNetworkingSockets/releases/tag/v1.6.1), [GNS architecture/features](https://github.com/ValveSoftware/GameNetworkingSockets), [Conan GNS](https://conan.io/center/recipes/gamenetworkingsockets), [MsQuic](https://github.com/microsoft/msquic).
- T20: [PostgreSQL 18.6](https://www.postgresql.org/docs/release/18.6/), [transactions](https://www.postgresql.org/docs/18/transaction-iso.html).
- T21: [Conan libpqxx](https://conan.io/center/recipes/libpqxx), [Conan libpq](https://conan.io/center/recipes/libpq), [libpqxx](https://github.com/jtv/libpqxx).
- T22: [Lua news](https://www.lua.org/news.html), [Lua 5.5 manual](https://www.lua.org/manual/5.5/manual.html), [Conan Lua](https://conan.io/center/recipes/lua), [Luau](https://luau.org/).
- T23: [OpenZeppelin ERC-20](https://docs.openzeppelin.com/contracts/5.x/erc20).
