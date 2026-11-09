# First implementation hand-off

Current checkpoint: **M2 remains open; M3 has not started.** The [closure evidence](../evidence/M2/closure/README.md) now verifies native transparency, corrected sampled WDDM accounting and passing current-host resize/reload budgets. Slang, full-ship bounds and the [exact longer/wider blue-white exhaust](../evidence/M2/closure/exhaust/README.md) have specific owner acceptance. The separate private asset replays byte-for-byte; original assets and the brand pack stay untouched. The owner now also explicitly accepts inspector/input. Remaining: one targeted MSVC Release compiled-UI/accepted-asset validation, CI recovery under unchanged pins/checks and normal PR integration. The independent exhaust/evidence follow-up now finds no actionable defect/new advisory; it is bounded agent advice, not human approval. No repeated compiler matrix, memory run or art reapproval is requested. Finish M2 before beginning M3.

Historical checkpoint (retained): the [GPU resource slice](../evidence/M2/gpu-resources/README.md) passes Linux Debug/Release/ASan/GCC resource ownership, transfer, retirement and clear-only resize checks. The owner's [Windows resource archives](../evidence/M2/windows-resources/README.md) now verify this code at `dbdd3ba` in MSVC/clang-cl Debug/Release; no new setup or rerun is requested. The [shader candidate](../evidence/M2/shader-interface/README.md) passes Linux/hosted checks and awaits new owner Windows/NVIDIA shader runs and compiler-choice acceptance; its test-only native fixture is not a shipping draw path. The [offline GLB importer](../evidence/M2/asset-import/README.md) now preserves the frigate's geometry/materials/lights and explicitly rebuilds tangents without changing its source. The [checked cooker/runtime package](../evidence/M2/cooked-scene/README.md) now loads without import tools and preserves full LOD0/all lights. Next: integrate tracked GPU residency/draws, fixed passes and inspector, then finish physical/platform/acceptance gates. M1 formal acceptance and M2 remain open. The planning brief below is historical, not a replacement for the current authorization or repository state.

This document prepares the next work; it does not authorize implementation during the present planning assignment. M0 is incomplete because the actual private checkout and raw hardware profile are unavailable here. The destination paths below are proposals and must be mapped to the checkout before editing. ADR numbers are provisional and may collide with existing records.

## Ready-to-use Codex prompt

> Continue NGEN from this planning package. Read the actual checkout's root and applicable nested AGENTS.md before repository operations. Read NGEN-PLAN.md, the repository audit, relevant existing implementation, and the existing ADR index. Use the project's current distill skill for major documentation. Preserve README layout, logos, primary headers and badges. Do not assume the project is empty or overwrite existing code to match this proposal.
>
> First complete M0's missing evidence using read-only inspection. Record HEAD/branch/dirty state, classify template-derived files against the appropriate ancestor, inspect workflows/rulesets/hooks/commitlint/PR template, inventory existing C++/build/test code and resolve licensing/policy conflicts. Capture the supplied raw Vulkan profile and a verified hardware inventory when those machines are available. Do not connect to infrastructure without a separately authorized infrastructure task.
>
> Reconcile this plan with the repository and propose only the smallest M1 delta. Keep facts, measurements, assumptions and proposed budgets separate. Accept no library version merely because it is listed in this report. Resolve a compatible pinned Conan graph for the declared Linux and Windows toolchains. Do not mark M0 complete while repository, policy or required hardware evidence is still missing; a delayed GPU experiment can remain a separate renderer gate if documented explicitly.
>
> If this new session explicitly authorizes M1 implementation after the audit, implement only the CPU foundation described below. Otherwise return the completed audit and exact proposed M1 changes. Do not start M2, add gameplay, create MMO services, implement blockchain or provision runners as incidental work.
>
> For authorized implementation, follow the actual Git Flow base and branch pattern `<type>/kyau-<six-hex>-description`, using `openssl rand -hex 3` for the suffix. Use only configured conventional types. Make detailed GPG-signed commits as `kyau`; never commit/push directly to main or develop. Keep signing, hooks, Gitleaks and commitlint enabled. If the signing identity/hardware is unavailable, finish reviewable local changes and report the specific blocked operation rather than bypassing the rule.
>
> Run meaningful relevant tests and obtain an independent code review. PR creation/merge uses `kyaulabs-bot` and the actual supplied template; formal review uses authorized `kyau` and the required approval text only after Test Plan completion. Verify that the merge strategy preserves required signed history and identities. Record exact evidence and remaining limits. No claim of passing a check without running it.

## M0 evidence to close first

| Item | Concrete result |
|---|---|
| Repository state | Exact SHA, branch, dirty files, remotes and relevant inventory; no reset/clean |
| Policy | Actual AGENTS and PR template; local/remote rules distinguished; allowed types, signatures, author/reviewer/bot merge semantics resolved |
| Template comparison | Each relevant file unchanged/customized/new/deleted/unknown; actual ancestor identified or absence documented |
| Licensing | Current NGEN license, copied material and ownership; intended engine/game split reviewed |
| Existing implementation | Map CMake/Conan targets, core/renderer/simulation/tests and reusable patterns |
| Dependency graph | Exact candidate versions, release dates, recipe revisions/options and compiler support, with successful minimal resolution/build evidence before acceptance |
| Hardware | Raw profile and physical-device inventory; GPU/VM assumptions marked until measured |
| Decision state | Only necessary foundational ADRs accepted, with owner/date/evidence; later research proposals remain open |

Useful read-only commands include `git status --short`, `git rev-parse HEAD`, `git branch --show-current`, `git ls-files`, and scoped `rg` searches. Run them only after reading the actual AGENTS.md. Do not use commands in this hand-off as permission to ignore more specific repository rules. Do not print credentials or signing material in logs.

## M1 scope

The outcome is a reproducible C++20 CPU foundation with a small test executable, explicit build boundaries and a minimal fixed-step command harness. It must be possible to build and run it without a GPU, display server, account, database, network service or asset-generation API.

| Work unit | Proposed files/targets | Verification and evidence |
|---|---|---|
| M1.1: preserve and map | Existing build/config plus minimal approved deltas | Focused diff against M0 findings; no unrelated replacements or README redesign |
| M1.2: dependency/toolchain lock | Existing Conan recipe or proposed `conanfile.py`, `conan.lock`, profile directory; shared CMake presets | Clean Linux Clang configure/build; controlled GCC/Windows probes; exact dependency graph/provenance report; mismatch produces useful error |
| M1.3: target boundary | Proposed `ngen_core` and CPU harness/test targets; target-scoped settings | Headless build in environment lacking Vulkan/SDL video/PostgreSQL; link graph/startup evidence |
| M1.4: value and lifetime primitives | Proposed core units, stable/generational IDs, clocks, error values and bounded scratch storage | Unit conversion/overflow, stale handle, allocation exhaustion and lifetime tests; no new generic framework |
| M1.5: bounded jobs and shutdown | Proposed jobs adapter and explicit worker budget, only where the core harness needs concurrency | Serial reference; bounded queue/capacity, cancellation, drain/shutdown and task-lifetime tests; no I/O blocking the worker pool; matches roadmap M1-C |
| M1.6: command/tick harness | Minimal value-only commands and fixed-step driver; in-process consumer | Same ordered command trace gives specified semantic state on repeated runs; wall-clock pacing excluded from game rules; no networking implementation |
| M1.7: test and diagnostics lane | Existing tests/CI patterns extended for owned-code warnings, formatting, unit tests, ASan/UBSan; TSan only where concurrency exists | Logs identify toolchain/revision; real error-path tests; initial coverage report with exclusions, no cosmetic percentage chasing |
| M1.8: evidence and review | Proposed `docs/evidence/M1/`, accepted ADR links and PR-ready Test Plan | Independent ownership/lifetime/build-boundary review; exact command output, dependency manifest and outstanding limits |

These work units refine the roadmap's M1 tasks (including M1-C jobs/shutdown); they are not a competing issue-number scheme. Split further only when each smaller change has a useful observable result. Do not install Jolt, EnTT, networking, SQL and every graphics tool into the first core target simply because they occur in the long-term matrix. A small CPU-only Jolt packaging probe may live in a disposable experiment to settle the headless/scale decision; production adapters belong when the flight slice needs them.

## Acceptance checks

1. A clean checkout can resolve the pinned dependency graph and configure/build/test through the agreed preset without hidden developer-machine state. Record the exact commands after presets exist; this document does not invent already-working repository commands.
2. Owned C++ code is C++20 and warning-free on the primary compiler. Compatibility exceptions are specific, justified and tracked; no blanket warning suppression hides owned-code defects.
3. CPU harness and tests start without graphics or MMO dependencies. Offline capability is proven by running with network access unavailable, not by saying the code probably never calls a service.
4. Value/ownership/error/overflow tests cover their specified failure modes. Sanitizer runs are separate from optimized timing evidence.
5. Repeating the simple command fixture gives its defined result; no cross-platform whole-physics determinism claim is made.
6. The diff meets the smallest-sufficient-change rule. No placeholder microservices, universal plugin system, speculative allocators or full ECS abstraction are added.
7. Evidence is reviewed and the actual PR Test Plan can be reproduced. Commits/review/merge remain subject to authorized identities, GPG and protected-branch rules.

Stop M1 at this boundary. M2 starts with one Vulkan surface/device/swapchain lifetime path and a fixed test scene, once its hardware gate is ready.
