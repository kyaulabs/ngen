# Requirements and deliverable traceability

Planning baseline: **2026-11-02**, supplied NGEN hand-off, sections 1–11. Links establish planning coverage. They do not establish implemented behavior.

**Status:** architecture, tasks and tests are **proposed**. Private audit is **blocked on checkout access**; implementation, dependency integration, hardware validation and benchmarks are **pending**. All sixteen ADRs require acceptance. M0 remains open.

## Engine, workflow and platform requirements

| Requirement | Contract and location | Milestone / completion evidence |
|---|---|---|
| Reusable C++20 engine; Linux primary, Windows secondary; Clang primary, GCC/MSVC compatibility | [Technology matrix][tech], [build workflow][build], ADRs 0001/0014 | M0-D, M1-A/F: resolved graph and clean compiler evidence. |
| Conan 2, CMake, Ninja; pinned profiles, recipes and reproducibility | [Build workflow][build] | M1-A/F: locks, presets, hashes, clean builds and explained nondeterminism. |
| Offline games independent of MMO; headless server independent of Vulkan | [Architecture diagrams][master], [core boundaries][render], ADR 0001 | M1-E, M5, M6-A: offline loop without services; no-display build/link inspection without graphics dependencies. |
| RAII, strong types, explicit ownership, predictable memory, bounded allocations, minimal abstractions | [Core and jobs contracts][render], ADRs 0005/0006 | M1-B–D: overflow, stale handles, capacity, cancellation, shutdown and immutable snapshots; allocation traces. |
| Zero warnings in owned code, meaningful tests/coverage, sanitizers, profiling, static analysis | [Build workflow][build], [benchmarks][bench] | M1-F onward: relevant checks on reviewed SHA; correctness and optimized performance runs separate. |
| Current AGENTS, Git Flow, authorized identities, signed conventional commits, branch format, PR template/Test Plan, hooks/rulesets/Gitleaks/commitlint | [Repository audit][audit], [build workflow][build], [first hand-off][first] | M0-A/B: actual policy and enforcement comparison; later authorized changes preserve all controls. |
| Preserve README layout/logos/headers/badges; smallest sufficient changes; independent milestone review; distill documentation | [Repository audit][audit], [build workflow][build] | Actual checkout inspection precedes changes; planning review does not establish repository acceptance. |
| Private engine; possible open-source engine with proprietary game; no modding | [License audit][audit], [script boundary][sim], ADRs 0013/0016 | M0 rights inventory and M13 release notices; engine/game separation does not itself resolve AGPL obligations. |
| No implementation, branches, commits, PRs, infrastructure or deployments in this assignment | [Executive scope][master], [first hand-off][first] | Planning only; private NGEN inspection remains outstanding. |

## Universe, flight and progression requirements

| Requirement | Contract and location | Milestone / completion evidence |
|---|---|---|
| One persistent logical galaxy; continuous km/AU/light-year coordinates; hierarchical precision | [Coordinate/authority specification][sim], ADRs 0003/0011 | M4-A–C, B3: normalized cells, frame epochs, analytic precision and rebase/history tests. Distribution follows M10. |
| Handcrafted story systems, procedural exploration, solar systems and predominantly fixed celestials | [World manifests and streaming][roadmap] | M4-E: fixed generation seeds, celestial placement, content identity and repeatable traversal. Moving planetary ephemerides are outside the initial model. |
| Security sectors, jump gates, wormholes and jump-capable ships | [Travel tasks][roadmap] | M4-D/E: explicit route type, sector permissions, jump eligibility/cost and unavailable-destination tests. |
| Seamless streaming and committed EVE-style warp | [Travel/precision contracts][sim], [asset streaming][render] | M4-D/E, B3: entry/transit/exit, prefetch, bounded residency and safe arrival delays; no visible loading-screen requirement becomes a zero-latency promise. |
| Landing/docking cinematics, orbital infrastructure and planetary hubs | [Miniature-universe tasks][roadmap] | M5-E: station/orbital/planetary approach, abort/departure, berth access and preserved cargo identity; surface physics is not required. |
| Newtonian movement beneath assistance; manual and tactical modes for supported classes; instant mode switching | [Flight specification][sim], ADR 0004 | M3-A/B: common force controller, class permissions, momentum-preserving switches and frame-rate independence. Server control transfer still requires authority acknowledgment. |
| Controllable fighters/drones from large ships; switching deployed vessels | [Control leases and fleet workload][sim] | M9-C, B5/B7: class/control leases, stale-input rejection, vessel identity and separate subordinate workload. |
| Configurable modular equipment/components; targeted and projectile weapons; component damage | [Physics/combat specification][sim] | M5-C, M9: fitted-component rules, line of sight, swept hits and bounded projectile handling; accepted damage cannot silently disappear. |
| Persistent wreckage, salvage and total ship loss | [Durable death contract][sim], [economy][economy] | M5-D/M7, B6: exactly one wreck, locked pending death, atomic cargo disposition and retry-safe salvage. |
| Free starter ships; repeatable recovery/progression missions | [Recovery/economy tasks][roadmap], [reward safeguards][economy] | M5-D/F: return to play after total loss; repeated mission rewards remain idempotent and cannot manufacture sellable starter value. |
| Authored introduction and guided onboarding before sandbox play | [Quest tasks][roadmap], [bounded scripting][sim] | M5-F, B4: recorded complete playthrough, saved transitions, bounded scripts and understandable progression. |
| Dynamic faction/career stories, strong PvE incentives and faction competition | [Faction/progression tasks][roadmap] | M7-F/M8: persisted branching career choices, faction-state consequences and repeatable PvE rewards with anti-farming tests; balance remains a measured design decision. |
| Player markets, crafting/industry, persistent inventory/resources/territory | [Transaction and industry contracts][sim], [economy][economy] | M5-A/B then M7-A–F, B6: reservations, fills/cancels, resource conservation, production and durable territory transitions. |

## Multiplayer, economy and operations requirements

| Requirement | Contract and location | Milestone / completion evidence |
|---|---|---|
| Server authority, prediction, reconciliation and bounded replication | [Network specification][sim], ADR 0009 | M6-A–F, B5: adverse network, replay, reconnect, identity and stale-control tests. |
| Hundreds then thousands; five figures aspirational | [Scale matrix][bench], [operations roadmap][build] | M13 staged 32→128→512→1,000-client experiments; launch admission remains below the demonstrated safe limit. Dispersed CCU does not establish battle capacity. |
| 200 fully equipped active AI ships, plus fighters/drones/projectiles; regional time dilation | [Fleet/clocks][sim], [B7][bench], ADR 0011 | M9: separately counted extras, dense-contact workload, CPU budgets, equal-simulation-time fixtures and boundary fairness tests. |
| Distant abstract NPC/economy simulation; local detail; offline manufacturing | [Industry and activation contracts][sim] | M7-D–F: reserved inputs, bounded catch-up and conserved identities across repeated activation/deactivation. |
| NPC expansion, warfare, production and territorial loss | [Faction tasks][roadmap] | M7-F adds durable minimal faction transitions; M8 extends warfare/territory rules and tests loss as well as acquisition. |
| Event-scoped PvP and environmental-grief prevention, attribution and mitigation | [Permission/damage rules][sim] | M8-A–E: versioned authorization and causal damage across collisions, debris, drones and indirect effects; unsafe mechanics remain restricted. |
| Future seamless authority transfer | [Migration protocol][sim], ADR 0011 | M10, B8: durable snapshot before ownership commit, atomic fencing, 10,000 fault transfers and measured presentation/authority pause. |
| Earned market currency; external real-money trading desirable but optional; currency alone on-chain | [Economy workstream][economy], ADR 0012 | M5/M7 closed-loop GameCredit first. M12 may adopt a separate public asset with fair funded conversion; free historical credits are never presumed backed. |
| Secure ledger, custody, asynchronous settlement, reconciliation, failure/reorg handling, sinks, bots/fraud and independent legal/security review | [Economy contracts and dated primary sources][economy] | M7 command corpus and crash invariants; M12 legal/custody/reserve/contract gates before public transfer. No tick waits for chain confirmation. |
| apollo/Docker/GTX and Windows RTX runners; isolated least privilege; no connection now | [Infrastructure plan][build], [hardware matrix][bench] | M0 captures and later authorized isolated-runner proof. GPU access, credentials, operator coverage and budgets remain unverified. |

## Rendering and development-art requirements

| Requirement | Contract and location | Milestone / completion evidence |
|---|---|---|
| Vulkan 1.3 candidate baseline, optional 1.4/features; Iris Xe/Wayland minimum, GTX VM and RTX Windows | [Renderer][render], [B0–B2][bench], ADR 0002 | Physical feature/format/WSI captures and 720p lightweight-flight tests; no shader FP64, RT or laptop battle-FPS requirement. |
| PBR, shadows/post, precise astronomical rendering, ships/asteroids/nebulae | [Renderer/asset specification][render] | M2–M4 first, M11 volumes later; reference materials, resource lifetime, origin/history and image/performance evidence. |
| GPU-driven counts, RT, advanced GI, volumetrics, DLSS/DLAA/FSR and temporal reconstruction | [Optional rendering tasks][roadmap], [technology matrix][tech] | M11-A–E: capabilities, licensing, quality/latency and low-tier/native/raster fallbacks. |
| Compare requested ECS/jobs/physics/shader/network/database/script/asset/profile/test libraries | [Cited technology matrix][tech], [renderer][render], [simulation][sim] | M0 compatibility probes; milestone-specific fit/performance tests. Package records are not integration results. |
| Development-time OpenAI 4K concepts, multiview references, separate 3D evaluation, Blender/UV/material/LOD/collision/cooker | [Art workstream][art], ADR 0015 | M2/M3 four-asset comparison, M11 fleet asset costs, M12/M13 rights checks; large-resolution API support carries its documented experimental caveat. |
| Prompt/model/version/cost/source provenance; no shipped credentials | [Art records and budget gates][art] | Retain outputs, permissions, request/usage metadata and accepted runtime assets; generation is separately authorized development work. No API spending occurred. |

## Sixteen required deliverables

| # | Deliverable and location | Planning status / remaining gate |
|---|---|---|
| 1 | [Repository/template audit][audit] | Public evidence recorded; private comparison incomplete, M0-A/B. |
| 2 | [Executive architecture brief][master] | Proposed; foundational decisions await M0. |
| 3 | [Cited technology matrix][tech] | Present; pins/recipes await integration. |
| 4 | [Module/dependency diagrams][master] | Proposed; headless/offline link proof at M1/M6. |
| 5 | [Sixteen ADR proposals][adrs] | Owner/evidence acceptance pending. |
| 6 | [Renderer/memory/jobs/assets][render] and [physics/coordinates/scripts/network/authority][sim] | Contracts proposed; milestone tests pending. |
| 7 | [Milestone dependency graph][roadmap] | Present; M11/M12 remain independently gated. |
| 8 | [Atomic tasks for M0–M13][roadmap] | Source areas await audit. |
| 9 | [Acceptance criteria][roadmap] and [tests][bench] | Defined targets; no executed passes. |
| 10 | [Hardware/performance matrix][bench] | Present; supplied hardware unverified. |
| 11 | [AI/PR/review/CI workflow][build] | Preserves supplied policy; actual enforcement pending. |
| 12 | [Ranked risks and experiments][bench] | Present; experiment evidence pending. |
| 13 | [Infrastructure/scaling][build] and [capacity stages][bench] | Proposed; no infrastructure modified. |
| 14 | [Blockchain/economic security][economy] | Separate feasibility track; public transfer disabled initially. |
| 15 | [OpenAI art/asset workstream][art] | Separate development track; no generation/spending. |
| 16 | [First M0/M1 hand-off][first] | Concrete next tasks; implementation requires separate authorization. |

Critical path: actual audit → CPU foundation → beautiful offline flight → miniature game → authority/persistence → fleet/distribution. Optional workstreams retain the requested future capabilities; scope changes require accepted decisions.

[master]: ../../NGEN-PLAN.md
[audit]: repository-audit.md
[tech]: technology-matrix.md
[render]: renderer-core-assets.md
[sim]: simulation-network-persistence.md
[roadmap]: milestones.md
[bench]: benchmarks-risks.md
[build]: build-review-infrastructure.md
[economy]: blockchain-economy.md
[art]: art-pipeline.md
[first]: first-implementation-handoff.md
[adrs]: ../adr/INDEX.md
