# Build, review and infrastructure

Status: proposed. All paths are candidates until the NGEN checkout audit maps existing structure. No runner or service was connected, provisioned or modified.

## Build contract

Use target-scoped CMake with C++20 and Ninja. Start with conventional headers and translation units; defer C++ language modules and a plugin ABI until a demonstrated requirement justifies them. Build `ngen_core`, a CPU-only simulation harness and tests first. Rendering, assets, the flagship game and hosted services are separately selected targets.

Candidate tools verified from official release metadata on 2026-11-02 are Conan 2.34.0, CMake 4.4.5, Ninja 1.13.2 and LLVM/Clang 23.1.4. These are adoption candidates, not a tested toolchain. Prefer the repository's working pins if present. M0 must test the complete dependency graph before locking a replacement. Keep compatibility CI on a recorded supported GCC release and the workstation's supported MSVC toolset; do not invent the workstation's installed version.

Sources: [Conan package history](https://pypi.org/project/conan/2.34.0/), [CMake release](https://github.com/Kitware/CMake/releases/tag/v4.4.5), [Ninja release](https://github.com/ninja-build/ninja/releases/tag/v1.13.2), [LLVM release](https://github.com/llvm/llvm-project/releases/tag/llvmorg-23.1.4).

Use Conan's `CMakeToolchain` and stable `CMakeDeps` initially. `CMakeConfigDeps` remains documented as experimental; evaluate it in an isolated dependency probe if a real recipe problem requires it. Do not globally opt into breaking generator behavior. [Conan generator status](https://docs.conan.io/2/reference/tools/cmake/cmakeconfigdeps.html)

Checked-in host/build profiles describe compiler version, C++20, architecture, standard library, runtime, build type and relevant options. Linux Clang starts with the chosen distribution's pinned libstdc++; Windows clang-cl and MSVC share a verified MSVC ABI/runtime configuration. Do not mix Debug and Release CRTs across a binary boundary. Do not use `-march=native` for distributed binaries. Enable optional SIMD only under an explicit supported CPU policy or runtime dispatch.

Commit a lock that covers all supported conditional dependency graphs and build requirements. Record recipe revisions, package IDs/revisions, source checksums, toolchain image digests and generated dependency graph reports. A Conan lock constrains dependency resolution; it does not by itself make binaries byte-identical. Use strict locks in release CI; partial locks belong only in controlled lock generation. [Conan lockfiles](https://docs.conan.io/2/tutorial/versioning/lockfiles.html)

Keep shared presets under version control and personal presets ignored. Proposed profiles: `linux-clang-debug`, `linux-clang-release`, `linux-asan-ubsan`, `linux-tsan`, `linux-gcc-compat`, `windows-clangcl-debug`, `windows-msvc-release`, and `server-headless`. Compiler identification and dependency resolution must fail with an actionable error when a preset cannot be satisfied. [CMake presets](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)

## Source and binary boundaries

| Target family | Allowed dependencies | Must not require |
|---|---|---|
| Core / coordinates / command types | C++20 standard library, selected small math/utilities | SDL, Vulkan, network, PostgreSQL, blockchain |
| Simulation / physics | Core, ECS, CPU physics, jobs, first-party scripting adapter | Window, GPU driver, database availability |
| Client presentation | Core, read-only render snapshots, SDL3, Vulkan, asset runtime | Database credentials or chain keys |
| Offline game | Simulation plus presentation and local save adapter | Account service, Internet access, MMO infrastructure |
| Dedicated server | Simulation, transport adapter, persistence client as configured | Vulkan headers/loader, SDL windowing, display server |
| Asset tools | Importers, shader compiler, image/mesh cookers | Runtime game credentials |
| Economy/settlement services | Database, service authentication, optional custody adapter | Direct mutation of simulation ECS or client-supplied balances |

The server preset must resolve and build successfully on a clean machine with neither Vulkan SDK nor display packages. Check its link graph and startup behavior; hiding a graphics flag while retaining transitive graphics linkage fails this gate.

## Errors, warnings and analysis

Treat owned-code warnings as errors using a curated warning set for each compiler. Mark external includes/dependencies appropriately; do not patch third-party projects solely to satisfy NGEN's warning policy. Require explicit ownership, RAII, bounded containers on hot paths and checked arithmetic for sizes/identifiers/currency. C++20 has no `std::expected`; reuse an existing result type or a small value/error type instead of silently requiring C++23.

Expected failures return typed errors. Assertions express programmer invariants and cannot replace validation of network, database, asset or script input. Do not impose global `-fno-exceptions` on dependencies without proving compatibility. Catch third-party exceptions at adapters; no exception crosses a worker thread, C callback or public module boundary. Lua's error model needs a separately verified RAII-safe boundary.

Use clang-format and clang-tidy versions matched to the pinned toolchain. Analyze owned code, sanitise meaningful execution paths, fuzz untrusted parsers and keep regression tests for actual risks. Start with Catch2 and Google Benchmark if existing repository choices do not already cover those needs. Do not maintain competing test frameworks.

## CI lanes

| Lane | Trigger / machine | Evidence and gate |
|---|---|---|
| Policy and source checks | Every PR on an isolated CPU runner | Hooks/policy equivalent, secret scan, formatting, lint, dependency/license diff; remove short-lived checkout credentials before executing PR code |
| Core and simulation | Every PR, Linux Clang | Unit/property/integration tests, ASan+UBSan run, CPU headless smoke; zero owned warnings |
| Concurrency | Jobs, simulation or ownership changes; Linux TSan | Race-free tested paths and useful trace; TSan is a separate build from ASan |
| Compatibility | Every merge candidate; Linux GCC and Windows MSVC/clang-cl | Build/tests using recorded toolchains and compatible dependency binaries |
| Vulkan correctness | Renderer changes on trusted Intel/NVIDIA runners | Validation-layer log, capability dump, fixed-scene render/readback, resize/minimise/device-loss behavior where injectable |
| Performance | Milestone gate and affected performance changes on a reserved reference machine | Release build, no sanitizers/validation, repeated fixed workload, raw frame/tick distributions and trace |
| Long soak / failure recovery | M6 onward, scheduled or release gate | Packet-loss tests, process crashes, persistence restore, bounded memory/queues, disconnect and recovery outcomes |
| Release | Protected job after reviewed commit | Signed source provenance, dependency/asset manifest, SBOM, reproducible cooker output and clean install smoke |

Sanitizer overhead is not a performance result. LLVM documents Linux support for TSan; do not promise a Windows TSan lane. ASan is supported on Windows with limitations, so keep Linux as the primary memory/race analysis environment. Add MSan only for a small fully instrumented dependency subset when a concrete risk warrants it. [ASan](https://clang.llvm.org/docs/AddressSanitizer.html), [TSan](https://clang.llvm.org/docs/ThreadSanitizer.html)

Coverage is diagnostic evidence. Proposed initial policy: report branch coverage and exercise every specified error/ownership/economic invariant; require at least 80% branch coverage in core units/coordinate/serialization modules after meaningful code exists. Exclude generated/vendor/platform glue with reasons. Do not set a global percentage that encourages trivial tests or claim a percentage proves correctness. Require explicit tests for each invariant and regression regardless of aggregate coverage.

## Self-hosted runner design

`apollo.kyaulabs` is a proposed host, not a verified capacity commitment. Confirm CPU/RAM/storage and GTX 1660 Super passthrough before assigning benchmarks. A VM with a virtual display is not evidence of NVIDIA Vulkan access. Use a disposable runner VM, separate nonprivileged account, short-lived registration, restricted network egress and no signing or personal keys. Docker by itself is not a hostile-code security boundary; do not give PR code a host Docker socket or homelab-management access.

Treat the RTX 5070 gaming workstation as trusted-code-only. Prefer a dedicated boot/VM environment where feasible; do not run arbitrary fork PRs inside Sean's normal desktop account. Reserve benchmark runs to avoid gaming, background jobs, thermal or power-state contamination. Capture OS, driver, clocks/power policy and connected GPU identity with every report.

Use least-privilege workflow tokens and pinned action commits, or a supported workflow dependency lockfile after verifying repository compatibility. Private checkout can use a short-lived read credential; disable persisted checkout credentials and remove them before executing PR code. Separate artifact publishing from untrusted test execution. Rebuild or validate artifacts at the trust boundary; never execute untrusted artifacts in a privileged follow-on job. Keep GPG/YubiKey signing on the authorized developer path. [GitHub workflow security and runner guidance](https://docs.github.com/en/actions/reference/security/secure-use)

## AI implementation and review contract

Each implementation task begins with the current `AGENTS.md`, relevant code and accepted ADR. A task states one observable outcome, permitted source areas, dependencies, non-goals, tests and evidence. Prefer existing implementations, standard facilities and existing dependencies before adding another layer. Parallel agents own distinct files and agree on shared types first; one integrator owns interfaces and merge order.

The author records why the change is needed and runs the relevant existing checks. An independent agent reviews the changed behavior, failure handling, ownership, lifetime, security and test evidence. This review is advice, not a fabricated GitHub approval or identity. Resolve blocking findings before the formal review.

After implementation is separately authorized, use the actual configured conventional type and branch pattern `<type>/kyau-<six-hex>-description`; branch from the policy-approved Git Flow base. Commits are authored/signed as authorized `kyau` with detailed bodies. Use `kyaulabs-bot` to create and merge PRs with the supplied template. Fill Summary, Changes by Phase, ADRs, Verification, applicable Architect Conditions, Commits and Test Plan. Attach exact command results, revision, environment and unresolved limits. Never mark a checkbox for a test that did not run.

Only `kyau` performs the formal review, with the required `✔️ Approved by: @kyau` text, after Test Plan completion and required checks. Verify that GitHub's self-review restrictions permit the proposed author/reviewer identity arrangement; if not, resolve policy legitimately rather than impersonating another account. Verify the merge strategy's resulting signatures and author/committer identities: bot squash/rebase can create new commits, so signed feature commits alone do not prove the merged history satisfies NGEN policy. Resolve that rule at M0; never silently substitute a web-flow signature for required authorized signing. Watch workflows, fix failures on the authorized branch, rerun affected gates and record the new evidence. Do not disable signing, hooks, Gitleaks or commitlint to force completion.

This planning task overrides standing defaults that might otherwise create branches, delete stale branches, push changes or merge PRs. None of those operations belongs in this turn.

## Production progression

1. **M1-M5:** development machines and disposable CI only. No always-on MMO service.
2. **M6:** one authoritative server process, a session/auth boundary and load-generator processes in an isolated staging environment. Establish bandwidth and tick budgets before estimating capacity.
3. **M7:** one PostgreSQL primary with tested backup/PITR, a bounded economy worker and transactional outbox. Use separate process roles before introducing a large service mesh. A single primary is a known availability limit, not HA.
4. **M8-M9:** admission control, battle reservations, rate limits, telemetry, backup restoration and incident procedures. Add database failover only after fencing and acknowledged-transaction durability are tested.
5. **M10:** partition by simulation ownership regions; migration coordinator with durable epochs, routing and recovery. Add nodes from measured utilization and migration costs, not player-count guesses.
6. **M13:** staged rollout, account/security operations, moderation, fraud response, on-call coverage, patch/rollback strategy and measured cost per concurrent user and per active region. Choose hosting providers and a dollar budget only after workload measurements and a separate spending decision.

The proposed initial alpha service objective is a 99.5% monthly availability target, durable acknowledged economy transactions within the documented failure domain, and a recovery drill restoring service within 60 minutes. These are planning targets, not existing guarantees. Public launch requires a business-approved SLO and a demonstrated restore/failover design; never call async backups alone zero-RPO.
