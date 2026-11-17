# Benchmarks, hardware validation and risks

All numeric limits here are proposed acceptance targets. No NGEN benchmark, dependency build or hardware validation was performed. Before a target becomes a milestone gate, pin the fixture and reference host. A measured failure requires an explanation and an approved scope/budget change; silently lowering the target is not a pass.

## Evidence format

Every run records source SHA, dependency lock/recipe revisions, executable hash, compiler/options, OS/kernel, driver, physical GPU UUID, CPU model/core quota, memory configuration, resolution/preset, workload/content hash, random seed, warmup and sampling periods. Record clocks/power/thermal state, median/p95/p99/max timing, sample count, peak/resident memory, allocations, queue depth, errors and trace location. Keep raw samples and logs with the summary.

Run correctness and speed separately. Use validation/sanitizers for correctness and optimized unsanitized builds for speed. Start with three warmed runs on a reserved machine, report run-to-run variance and retain outliers. Longer soak tests examine growth and rare failures; averages cannot establish bounded memory or latency. Software Vulkan can support smoke tests but cannot validate the requested GPUs.

## Hardware matrix

| Hardware | Supplied description | Required validation | Assigned role |
|---|---|---|---|
| X1 Carbon Gen 9 | i7-1185G7, Iris Xe Tiger Lake GT2, 16 GiB, Arch/Wayland, Mesa 26.2.6; advertised Vulkan 1.4.354 | Raw profile plus current physical-device dump, actual device/driver/format/features, Wayland WSI, shared-memory pressure, thermals/power | Minimum functional renderer and lightweight flight; not the 200-ship battle gate |
| Linux VM / apollo candidate | GTX 1660 Super mentioned; CPU, RAM, VM topology and GPU access unknown | Guest device identity, passthrough versus software rasterizer, Vulkan smoke/capture, CPU quota/storage and isolation | Linux NVIDIA compatibility and medium rendering only after validation; server benchmark only after CPU is declared |
| Windows workstation | RTX 5070; CPU/RAM/driver not supplied | Physical device/driver, Windows toolchain/CRT, capture and optional features, reserved benchmark environment | Windows compatibility, high renderer and optional RT/reconstruction experiments |
| Dedicated CPU reference | Not selected | CPU/core quota, RAM, storage/fsync, networking, OS and power limits | Simulation/network/database performance; physical GPU irrelevant |
| Disposable CPU CI | Not provisioned in this assignment | Clean build environment, no display/SDK requirement, limited credentials | Core tests, sanitizer/fuzz checks and headless dependency proof |

Hardware facts above come from the hand-off. They are not newly verified measurements. The reported Vulkan version cannot replace physical-device feature checks. [Khronos version distinction](https://docs.vulkan.org/guide/latest/versions.html)

## Benchmark definitions

| ID / gate | Workload | Proposed acceptance and required evidence |
|---|---|---|
| B0: capability and lifetime, M0/M2 | Actual device capture; missing-feature masks; 1,000 resize/minimise cycles; repeated partial-init/load/unload; 30-minute smoke | Correct GPU/WSI; required features available; zero validation/lifetime errors and bounded memory. Unsupported optional features produce a working basic path. Fault injection is distinguished from real device loss. |
| B1: laptop flight, M3 | 1280×720 low; one detailed player ship; 100 instanced asteroids; ≤250k visible triangles; one 1024 shadow map; no RT; ≤256 MiB texture working set | Median ≤50 ms, p95 ≤66.7 ms, p99 ≤100 ms; no streaming hitch >200 ms after warmup. Provisional GPU allocation cap 1 GiB, process working-set cap 6 GiB; measure total pressure without double-counting UMA pages. Fixed camera/input replay and content hashes. |
| B2: desktop flight, M3 | Same scene hash at 1920×1080; medium GTX 1660 Super / high RTX 5070 presets separately recorded | Proposed p95 ≤33.3 ms on verified GTX VM and ≤16.7 ms on RTX. A preset difference is explicit, not a claim of directly comparable GPU speed. |
| B3: coordinates and warp, M4 | Analytic frame fixtures at 0, 1 AU, 1 ly and galaxy-scale cell coordinates; 10,000 rebase round trips; 10 m/100 m/1 km hulls; committed warp and destination stalls | Coordinate transform residual ≤1 mm and velocity residual ≤0.1 mm/s for analytic fixtures; projected image difference ≤0.25 px in fixed render fixtures. No frame-epoch mixing, nonfinite values or history spike. Contact tolerance and supported hull/speed limits are separately measured, not inferred from transform accuracy. |
| B4: miniature universe, M5 | Scripted onboarding, flight, mining, combat, total loss, recovery, salvage, docking, local trading, save/reload and return travel | Entire loop playable offline with no sockets/account/DB service. Repeated rewards and inventory transfers preserve identities and quantities; invalid saves/scripts fail explicitly; one-hour play soak has stable memory. Local market actions target p95 ≤20 ms on declared host, separately from file-save latency. |
| B5: multiplayer, M6 | 32 clients; scripted flight/control changes; 150 ms RTT, 30 ms jitter, 2% packet loss, reordering; 30 minutes | Zero unauthorized commands, duplicate durable effects, permanent desync or crashes. Defined free-flight trace at ≤100 m/s: p95 correction <0.5 m after warmup; collision corrections separate. Record latency and bandwidth distribution; next scaling step requires measured per-client/region budgets. |
| B6: persistence/economy, M7 | 1,000,000 commands grouped into generated sequences with fixed seeds; concurrent order fills/cancels, transfers, death, salvage and production; crashes around every commit/outbox boundary | Zero unexplained imbalance, negative player balances, duplicate item or repeated reward. Acknowledged effects survive the stated failure model. Restore and replay to a consistent position. Initial service goal: p95 durable local-region command ≤100 ms under 100 mixed commands/s on pinned host; contention and retry counts reported. |
| B7: fleet and time dilation, M9 | 200 fully equipped AI ships, plus 800 active drones/fighters, 10,000 concurrent damaging projectiles and 5,000 static collision shapes. Pin dimensions, weapons/fire rates, ranges, AI cadence and contact density | CPU-reference target p99 complete region tick ≤25 ms at base 30 Hz for 30 minutes. Bounded memory/queues and no loss of accepted damaging events. Run sparse and dense-contact variants. At scales 1, 0.5, 0.25, a closed fixture with identical tick-indexed commands, RNG and controlled external economy events agrees at equal simulation time within declared tolerance, while CPU work/wall-second decreases. Test wall-time manufacturing, transfers and reinforcement admission separately for exploits. No laptop battle-FPS promise. |
| B8: migration, M10 | 10,000 transfers; destination unavailable, source/destination kill, duplicate handoff, old-owner pause/resume, network partition and late writes | At most one valid writer per entity across all epochs at any instant; old-epoch mutations fail atomically after ownership changes. Zero duplicate inventory/reward effects. Transfer payload is durably recoverable and verified before owner/epoch commit; kill the source immediately after commit to prove recovery. Warm healthy-target aim: p95 authority pause ≤500 ms while presentation continues; safety may require a longer pause under faults. Record both healthy and failure tails. |
| B9: security and operations, M13 | Auth/session abuse, packet/asset/script fuzz corpus, exhausted queues, backup loss, database failover, credential rotation, build/asset provenance and patch rollback drill | No unresolved critical findings in authorized independent review; no secrets in client/assets; restored service within provisional 60-minute alpha RTO; documented RPO for each failure domain. Any real-value transfer gate additionally requires settlement/legal/security evidence. |

The flight and fleet workloads measure different things. 200 AI ships does not prove 200 connected clients. A universe with 5,000 clients spread over many quiet systems does not prove a 5,000-player battle. Extend the scale matrix in stages: 32 → 128 → 512 → 1,000 clients, then larger only with a recorded cost/latency model. For each stage run dispersed, clustered and sudden-arrival scenarios. Keep admission limits below the last demonstrated safe boundary.

An illustrative replication cost is 200 entities × 80 bytes × 20 updates/s = 320 kB/s per observer before overhead. With 200 observers this is 64 MB/s server egress before drones, headers or retransmissions. This is arithmetic, not the proposed wire format. Interest management, quantization and projectile lifecycle messages must be measured as part of B5/B7.

## Ranked risk register

Probability and impact are engineering judgments, not statistical estimates. P0 blocks an early architecture commitment; P1 blocks the named feature; P2 can remain deferred.

| Rank | Risk / likelihood / impact | Earliest experiment and evidence | Response / owner |
|---|---|---|---|
| P0-01 | Private checkout differs from plan; high / high | M0 exact inventory, current AGENTS, template classification | Adapt plan to existing code; Sean + integration reviewer |
| P0-02 | Template license conflicts with intended proprietary use; medium / high | M0 license/copyright/contribution inventory and decision | Resolve rights/license before distribution; Sean + qualified reviewer |
| P0-03 | Minimum GPU/WSI or VM access differs from hand-off; medium / high | B0 physical-device capture and first light | Adjust capability tier or hardware role explicitly; renderer owner |
| P0-04 | Large hulls/high-speed contacts exceed local physics fidelity; high / high | B3 hull-scale, relative-velocity and docking tests | Restrict supported contact scale, use compound/proxy design or compare physics engine; simulation owner |
| P0-05 | Core depends on graphics/services; medium / high | Clean headless build without Vulkan/SDL video/DB | Correct target edges before M3; core owner |
| P0-06 | Latest library graph fails chosen toolchains; medium / high | M0/M1 clean Conan resolution and minimal links on Linux/Windows | Pin a verified graph; minimal vetted recipe or older compatible release with rationale; build owner |
| P1-01 | Rendering/asset scope consumes project before playable flight; high / high | B1 plus one approved hull/material pipeline | Stop feature expansion until M3; technical lead/art owner |
| P1-02 | Rebase works for meshes but breaks motion vectors/physics/history; high / high | B3 continuous camera/ship/particle/shadow traces | Apply one frame-epoch transform to all consumers; renderer + simulation |
| P1-03 | Networking gives jitter or allows ownership forgery; medium / high | B5 with stale control versions and reconnect | Fix flight model/auth/reconciliation before scaling; network owner |
| P1-04 | Currency/items duplicate under retries, deaths or recovery; high / critical | B6 concurrent fault corpus and state reconstruction | Block value-changing feature release; persistence owner |
| P1-05 | Offline/active NPC conversion duplicates resources; medium / high | Repeated materialize/dematerialize with reserved jobs | One underlying inventory/job identity; economy owner |
| P1-06 | Time dilation mixes clocks or fails to reduce work; medium / high | B7 equal-sim-time outcomes and wall-time CPU telemetry | Explicit clock assignment, lower update cadence, admission control; simulation owner |
| P1-07 | Neutral players harmed through indirect mechanics; high / high | Event/collision/drone/debris/tractor causality corpus | Restrict harmful mechanic where proof is inadequate; game design/security |
| P1-08 | Multiple authorities survive migration/failover; medium / critical | B8 paused old-owner and atomic-fence races | Durable epoch enforcement at every write recipient; distributed owner |
| P1-09 | Local runner compromise reaches personal/homelab systems; medium / critical | Isolated-runner threat model and credential/egress test | Ephemeral isolation, trust-separated jobs; infrastructure owner |
| P1-10 | Signatures/identities lost by bot merge strategy; medium / high | M0 policy-consistent dry-run in a disposable test repository when separately authorized | Choose compatible signed-history strategy; never bypass checks; Sean |
| P2-01 | Token compliance/custody operations outweigh game resources; high / critical | M12 money-flow/cost/legal/security feasibility | Keep external transfer disabled; preserve playable economy; business/legal owner |
| P2-02 | Generated 3D costs more cleanup than manual modeling; high / medium | Four-asset comparison measuring accepted artist-hours | Keep Blender/manual baseline; art owner |
| P2-03 | Asset rights or provenance cannot support distribution; medium / high | Per-asset/tool/model rights review before acceptance | Replace or relicense affected asset; art/legal owner |
| P2-04 | Five-figure concurrency lacks a sustainable cost model; high / high | Dispersed/clustered arrival load and cost measurements after M10 | Keep tested admission cap and smaller launch; operations owner |

## Decision order

Resolve P0 evidence and dependency boundaries first. Build the flight slice next, using small precision/contact probes where needed to avoid a wrong foundation. Prove game-state invariants before adding durable multiplayer value. Prove local fleet capacity before distributing interacting regions. Chain settlement and advanced rendering may progress independently, but neither can substitute for a playable, recoverable core.

The author of each experiment owns its raw evidence; an independent reviewer owns the pass/fail recommendation. Sean accepts material scope or budget changes. A checklist entry without its fixture, environment and result remains open.
