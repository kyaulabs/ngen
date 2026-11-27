# NGEN execution order

Use this file when asked to continue the roadmap. It gives one single-stream
order for every remaining issue on [NGEN Roadmap Project #4][board], with creator
work interleaved into the flagship milestones rather than postponed until the end.
**Handoff** rows mark work moving into the Space MMO repo or an independently
deployed service. Letter suffixes insert those checkpoints without renumbering
existing steps.

Reference snapshot: **2026-11-17**, repository baseline `7dfe1c2f`, twelve open
milestones and **67 open issues / 67 Todo cards**. Membership was checked against
actual issues and project cards, not cached milestone counters. The accepted
M1–M3 foundation and published source-only 0.1.0/0.1.1 are the baseline; do not
restart them or relabel historical evidence. Individual ADR statuses remain intact.

The linked issues own each task's outcome, dependencies, proposed source areas and
acceptance evidence. [Milestones](milestones.md) owns integrated acceptance;
[creator scope](creator-toolchain.md) defines C1/C2 boundaries. This file adds an
execution sequence, not replacement requirements, new tracker records or a blanket
implementation/spending/deployment authorization. This planning request starts no
engine, game or MCP implementation.

## Current position

| Field | Value |
|---|---|
| Last verified step | **21 — [C2-D #80](https://github.com/kyaulabs/ngen/issues/80)**; [owned board playtest](../evidence/C2/owned-playtest.md). |
| Open gate | **M5 integrated acceptance**; the B4 technical gate and recorded owner playthrough with reload are verified. The independent gameplay/economy review is pending. |
| Next numbered step | **22 — [C2-E #81](https://github.com/kyaulabs/ngen/issues/81)**; integrated creator workflow and abuse containment over real MCP. |
| Optional selections | M11 features and M12 settlement are not selected by this schedule. |

Resume request: **“Read `docs/planning/execution-order.md` and continue from the
next incomplete step.”** A later implementation request supplies the work scope;
within that authorization, continue normally without per-command approval prompts.

## How to follow the order

1. Refresh the next issue, its discussion, project status and actual checkout.
   Confirm linked dependencies and entry decisions from evidence, not just a
   closed-card count. Reuse working code instead of implementing it twice.
2. Work in ascending step order. These are scheduling choices among valid
   dependencies, not a change to GitHub's dependency graph. In particular, full
   C1/C2 completion before M6 is the chosen schedule; the existing hard creator
   prerequisite for M5 is C1-A/B.
3. Before each implementation, map the issue's proposed paths to the actual code,
   select its public test seam and use the project's real native commands. Future
   tool/API names and test targets must not be invented in advance. Follow TDD,
   applicable checks, review and normal signed/approved Git Flow.
4. Mark a row complete only after its actual acceptance and required integration
   checks. Update the current-position fields and add a brief source/PR/evidence
   link in the progress record. A merged PR alone does not close a milestone.
5. At each checkpoint, complete the integrated scenario and required reviews from
   `milestones.md` before crossing its dependent entry gate. Preserve resource,
   licensing, offline/headless and source-bound hardware/evidence limits.
6. If a prerequisite or task changes, record the blocker and revise the order
   visibly. Do not silently skip it, weaken a check or expand scope. A later
   independent task may be rescheduled with an explained dependency-safe update.

`[ ]` means uncompleted work, not permission to execute it. At optional steps,
record **selected**, **deferred** or **rejected**; do not mark unperformed work done.
On resumption, the next incomplete step means the first incomplete **applicable**
row. Explicitly deferred/rejected optional rows are not eligible for the current
release, even though their boxes remain unchecked. No dates, team size, paid
services or capacity promise are implied.

## 01–10: SDK baseline, precision and astronomical travel

Start with a minimal external engine consumer; do not freeze provisional world or
coordinate APIs before the M4 precision decision. Reuse the current inspector and
asset/build paths. Establish human-usable operations before exposing them to MCP.

| Step | Done | Issue | Outcome |
|---|---|---|---|
| 01 | [x] | [C1-A #72](https://github.com/kyaulabs/ngen/issues/72) | Expose the minimal consumable engine SDK boundary. |
| 02 | [x] | [C1-B #73](https://github.com/kyaulabs/ngen/issues/73) | Create, validate, build/cook/test/run/package one standalone project. |
| 03 | [x] | [M4-A #11](https://github.com/kyaulabs/ngen/issues/11) | Implement canonical cell/offset coordinates and explicit nonrotating frame transforms. |
| 04 | [x] | [M4-B #12](https://github.com/kyaulabs/ngen/issues/12) | Transfer one local physics domain between origins at a tick barrier. |
| 05 | [x] | [C1-C #74](https://github.com/kyaulabs/ngen/issues/74) | Author and save a bounded reusable 3D scene. |
| 05a | [x] | [M4-F #97](https://github.com/kyaulabs/ngen/issues/97) | Simulate and render a bounded cosmetic particle fixture across a live origin transfer. |
| 05b | [x] | [M4-G #98](https://github.com/kyaulabs/ngen/issues/98) | Produce and read back GPU motion vectors across frame epochs. |
| 06 | [x] | [M4-C #13](https://github.com/kyaulabs/ngen/issues/13) | Verify combined rendering and prediction history after M4-F/G. |
| 07 | [x] | [M4-D #14](https://github.com/kyaulabs/ngen/issues/14) | Implement one committed warp trajectory with entry/transit/exit states. |
| 08 | [x] | [C1-D #55](https://github.com/kyaulabs/ngen/issues/55) | Connect owned play/inspection/reload and remove one measured authoring bottleneck. |
| 09 | [x] | [C2-A #77](https://github.com/kyaulabs/ngen/issues/77) | Select/pin a local MCP protocol/SDK and expose scoped discovery/read operations. |
| 10 | [x] | [M4-E #15](https://github.com/kyaulabs/ngen/issues/15) | Stream connected authored/procedural system manifests with prefetch, proxies and bounded eviction. |

**Before 01/02/09:** record the scoped ADR 0018 decisions; do not ratify unrelated
proposals or build a speculative universal plugin/command framework.
**Before 03:** the owner accepted the scoped ADR 0003 arithmetic contract and
entry model. **Before 04:** agree on contact tolerances and the representative
1 km fixture. The analytic evidence does not accept live origin transfers.
**Before 06:** finish and verify both M4-F and M4-G. Keep their rendering
and GPU readback separate from the existing CPU-projected motion evidence.
**After 10:** close M4's integrated two-system traversal, origin/history, streaming
failure and budget gates. Verify the fixture’s one protected system and one lawless
system, including protected NPC starts and no sector overrides. This is manifest
data, not an assertion that M8 police are implemented.

## 11–22: Miniature offline game, usable creators and complete local MCP

Use the game to exercise real authoring needs. The non-space sample independently
checks that the engine is reusable. MCP is an adapter to C1 operations, not a second
validation path or a shortcut to host/production authority.

| Step | Done | Issue | Outcome |
|---|---|---|---|
| 11 | [x] | [M5-A #16](https://github.com/kyaulabs/ngen/issues/16) | Apply one local inventory/currency transfer through an idempotent transaction kernel. |
| 12 | [x] | [M5-B #17](https://github.com/kyaulabs/ngen/issues/17) | Make a mining command reserve and transfer a bounded resource yield. |
| 13 | [x] | [M5-C #18](https://github.com/kyaulabs/ngen/issues/18) | Resolve the two fixture weapons against hull/component health through one damage path. |
| 14 | [x] | [C2-B #78](https://github.com/kyaulabs/ngen/issues/78) | Apply project/source/scene edits through validated creator operations. |
| 15 | [x] | [M5-D #19](https://github.com/kyaulabs/ngen/issues/19) | Transition a destroyed fixture ship into one wreck and one repeatable starter recovery entitlement. |
| 16 | [x] | [M5-E #20](https://github.com/kyaulabs/ngen/issues/20) | Permit the fixture market transaction through a docking state machine. |
| 17 | [x] | [C2-C #79](https://github.com/kyaulabs/ngen/issues/79) | Run bounded authorized build/cook/test/package jobs. |
| 18 | [x] | [M5-F #21](https://github.com/kyaulabs/ngen/issues/21) | Run and save one bounded first-party faction/career quest through the playable loop. |
| 19 | [x] | [C1-E #75](https://github.com/kyaulabs/ngen/issues/75) | Publish redistributable space/non-space samples and task-oriented SDK documentation. |
| 20 | [x] | [C1-F #76](https://github.com/kyaulabs/ngen/issues/76) | Accept an independently used creator workflow and packaged non-space game. |
| 21 | [x] | [C2-D #80](https://github.com/kyaulabs/ngen/issues/80) | Inspect/control only an owned development playtest. |
| 22 | [ ] | [C2-E #81](https://github.com/kyaulabs/ngen/issues/81) | Prove game creation and abuse containment through real MCP messages. |
| 22a | [ ] | [M6 handoff #131](https://github.com/kyaulabs/ngen/issues/131) | Start runnable Space MMO development in its own repo against a pinned NGEN release. |

**Before 17/21:** establish explicit project execution authority, proven host
isolation, bounded owned jobs and development sessions. No credential inheritance,
unrestricted shell, live shard or implicit network/spending/publication grant.
**After 18:** close M5's saved one-hour offline loop, recovery/progression/invariant
gates and recorded owner playthrough. The [windowed voyage checkpoint](../evidence/M5/windowed-voyage.md)
uses live controls and a streamed frontier landmark over the
[saved native voyage](../evidence/M5/saved-voyage.md). The [one-hour B4 result](../evidence/M5/b4-soak-results.md)
met the flight, memory and separate market-latency budgets. The owner playthrough
and reload were verified locally; the recordings and save are not published.
The independent gameplay/economy review still gates M5. Teaching security
rules is not a substitute for M8 enforcement.

**Sequencing exception:** The owner authorized step 19 while that review is
pending. C1-E depends on C1-A–D, not M5 acceptance. This does not waive the
review or permit step 22a or runnable multiplayer work.
**After 20:** accept the independently used C1 workflow and supported sample/package
results. **After 22:** accept C2's real protocol/authoring/build/play and abuse/cleanup
checks. A scripted MCP client is protocol evidence, not fabricated AI-host usage.
C1/C2 releases can ship independently of MMO production, when separately authorized.

## 23–28: Authoritative multiplayer

After M5 and full C1/C2 acceptance, finish **22a before 23**: prove the offline
slice from a clean Space MMO checkout before assembling its server. Game design
and website work can start earlier. Keep transport and database dependencies out
of the offline game and generic engine path.

| Step | Done | Issue | Outcome |
|---|---|---|---|
| 23 | [ ] | [M6-A #22](https://github.com/kyaulabs/ngen/issues/22) | Assemble the M5 simulation as a headless server executable. |
| 24 | [ ] | [M6-B #23](https://github.com/kyaulabs/ngen/issues/23) | Authenticate a session over the pinned GameNetworkingSockets adapter. |
| 25 | [ ] | [M6-C #24](https://github.com/kyaulabs/ngen/issues/24) | Accept bounded versioned inputs and return authoritative snapshots/receipts. |
| 26 | [ ] | [M6-D #25](https://github.com/kyaulabs/ngen/issues/25) | Predict and reconcile the locally controlled ship. |
| 27 | [ ] | [M6-E #26](https://github.com/kyaulabs/ngen/issues/26) | Replicate interest-filtered state within explicit per-client budgets. |
| 28 | [ ] | [M6-F #27](https://github.com/kyaulabs/ngen/issues/27) | Execute the 32-client adverse-network scenario and retain a reproducible harness. |
| 28a | [ ] | [M6 handoff #132](https://github.com/kyaulabs/ngen/issues/132) | Verify the Space MMO client and headless server adopt M6 on the same NGEN version. |

**After 28:** accept the integrated headless/authority, session, reconciliation and
adverse-network scenario with its declared rates/budgets; do not infer cross-platform
physics determinism or larger-player capacity. Finish **28a before 29**.

## 29–35: Durable economy, property and faction simulation

Property follows the durable transfer/checkpoint foundation, before faction and
sovereignty work builds on it. Keep asset title separate from jurisdiction and the
server process's authority to write state.

| Step | Done | Issue | Outcome |
|---|---|---|---|
| 29 | [ ] | [M7-A #28](https://github.com/kyaulabs/ngen/issues/28) | Commit one durable transfer with journal, receipt and transactional outbox. |
| 30 | [ ] | [M7-B #29](https://github.com/kyaulabs/ngen/issues/29) | Restore a versioned world checkpoint plus subsequent durable outcomes. |
| 31 | [ ] | [M7-G #71](https://github.com/kyaulabs/ngen/issues/71) | Persist versioned property titles and resource/access rights for stations, planets, moons and sectors. |
| 32 | [ ] | [M7-C #30](https://github.com/kyaulabs/ngen/issues/30) | Make one station market fill atomic under concurrent orders. |
| 33 | [ ] | [M7-D #31](https://github.com/kyaulabs/ngen/issues/31) | Complete one offline manufacturing job from reserved inputs. |
| 34 | [ ] | [M7-E #32](https://github.com/kyaulabs/ngen/issues/32) | Convert one fleet/factory between strategic and detailed representation. |
| 35 | [ ] | [M7-F #33](https://github.com/kyaulabs/ngen/issues/33) | Run one bounded NPC faction decision loop that commits a durable territory transition. |
| 35a | [ ] | [M7 handoff #133](https://github.com/kyaulabs/ngen/issues/133) | Adopt the durable economy and prove a trade survives restore in game-owned staging. |

**After 35:** close M7's generated-command conservation, crash/retry, restore,
industry-clock and title/resource gates. State actual durability guarantees; no
simulation-thread DB waits or presumed host-loss protection. Finish **35a before
36**; the offline game and NGEN SDK still do not require PostgreSQL.

## 36–40: Protected-space law, NPC police and lawless sovereignty

Protected space is punitive, not invulnerable. Lawless combat does not authorize
forged commands or economic duplication. Starter NPC jurisdiction remains binding.

| Step | Done | Issue | Outcome |
|---|---|---|---|
| 36 | [ ] | [M8-A #34](https://github.com/kyaulabs/ngen/issues/34) | Classify one bounded combat fixture under versioned security/crime rules. |
| 37 | [ ] | [M8-B #35](https://github.com/kyaulabs/ngen/issues/35) | Dispatch and resolve one overwhelming NPC police response to criminal aggression. |
| 38 | [ ] | [M8-C #36](https://github.com/kyaulabs/ngen/issues/36) | Attribute indirect aggression and enforce explicit anti-exploit rules. |
| 39 | [ ] | [M8-D #37](https://github.com/kyaulabs/ngen/issues/37) | Commit one player claim/capture/transfer through property and sovereignty rules. |
| 40 | [ ] | [M8-E #38](https://github.com/kyaulabs/ngen/issues/38) | Run an adversarial security/sovereignty replay. |

**Before 37/39:** resolve the issue's pursuit/escape/response timing and property/
child-rights policy questions. **After 40:** accept the complete protected/lawless,
police, causal attribution and durable-title replay with security/game-rule review.
Zero unauthorized state mutation is required; zero victim damage is not the policy.

## 41–46: Measured fleet scale and time dilation

| Step | Done | Issue | Outcome |
|---|---|---|---|
| 41 | [ ] | [M9-A #39](https://github.com/kyaulabs/ngen/issues/39) | Encode the complete fleet workload as a seeded scenario. |
| 42 | [ ] | [M9-B #40](https://github.com/kyaulabs/ngen/issues/40) | Apply explicit admission and saturation policy at every bounded fleet resource. |
| 43 | [ ] | [M9-C #41](https://github.com/kyaulabs/ngen/issues/41) | Deploy one controllable fighter and transfer pilot control through a versioned lease. |
| 44 | [ ] | [M9-D #42](https://github.com/kyaulabs/ngen/issues/42) | Remove the largest measured fleet bottleneck without changing semantics. |
| 45 | [ ] | [M9-E #43](https://github.com/kyaulabs/ngen/issues/43) | Advance fixed simulation steps at an announced regional time scale. |
| 46 | [ ] | [M9-F #44](https://github.com/kyaulabs/ngen/issues/44) | Execute the integrated fleet and overload soak on recorded hardware. |

**Before 44:** obtain the actual bottleneck profile. **Before 45:** resolve the
applicable clock ADR. **After 46:** accept the fixed workload, saturation/fairness,
performance and overload evidence without reducing the declared activity to pass.

## 47–51: Fenced regional handoff

Do this only after M9 proves the need and local capacity limits; not as a speculative
replacement for the single-process authoritative server.

| Step | Done | Issue | Outcome |
|---|---|---|---|
| 47 | [ ] | [M10-A #45](https://github.com/kyaulabs/ngen/issues/45) | Enforce monotonically increasing region epochs at all authoritative write boundaries. |
| 48 | [ ] | [M10-B #46](https://github.com/kyaulabs/ngen/issues/46) | Publish one durably recoverable migration snapshot at an exact tick boundary. |
| 49 | [ ] | [M10-C #47](https://github.com/kyaulabs/ngen/issues/47) | Implement prepare/quiesce/commit/resume for one region transfer. |
| 50 | [ ] | [M10-D #48](https://github.com/kyaulabs/ngen/issues/48) | Route clients across the committed owner change without resetting identity. |
| 51 | [ ] | [M10-E #49](https://github.com/kyaulabs/ngen/issues/49) | Execute 10,000 fault-injected transfers and a dense-boundary scenario. |

**After 51:** accept fencing, durable handoff/recovery, partition and measured-pause
evidence with the required distributed-systems review. Two processes do not prove
five-figure concurrency.

## 52–56: Conditional M11 graphics selection

**Decision checkpoint:** explicitly select or defer each feature using measured
benefit/platform/licensing evidence. Unselected work does not block the game-only
path. If selected, execute the applicable rows in the order below and accept their
basic-path/resource regressions before M13. If deferred/rejected, record the choice
without checking its box or claiming its feature passed. Earlier research is
possible, but rescheduling requires an explicit dependency-safe update to this file.

| Step | Done | Issue | Outcome |
|---|---|---|---|
| 52 | [ ] | [M11-A #50](https://github.com/kyaulabs/ngen/issues/50) | Adopt a pass graph only after a fixed-schedule versus proven-library comparison. |
| 53 | [ ] | [M11-B #51](https://github.com/kyaulabs/ngen/issues/51) | Move one measured culling/LOD workload to GPU-generated indirect draws. |
| 54 | [ ] | [M11-C #52](https://github.com/kyaulabs/ngen/issues/52) | Integrate one selected DLSS/DLAA/FSR backend through a stable input contract. |
| 55 | [ ] | [M11-D #53](https://github.com/kyaulabs/ngen/issues/53) | Add one selected GI or ray-traced effect with a tested basic lighting path. |
| 56 | [ ] | [M11-E #54](https://github.com/kyaulabs/ngen/issues/54) | Add one bounded nebula/volumetric effect to the astronomical scene. |

## 57–61: Conditional M12 settlement decision

**Decision checkpoint:** choose the no-transfer game release or separately authorize
this workstream. M12 failure/rejection must not block a game-only M13. If selected,
use this order; each legal, custody, experiment, cost and public-transfer permission
remains separate. Earlier authorized feasibility research is possible, not assumed.

| Step | Done | Issue | Outcome |
|---|---|---|---|
| 57 | [ ] | [M12-A #56](https://github.com/kyaulabs/ngen/issues/56) | Obtain a written feasibility decision for the exact token and custody model. |
| 57a | [ ] | [M12 handoff #134](https://github.com/kyaulabs/ngen/issues/134) | Record the optional settlement service boundary and decide its repo placement at M12. |
| 58 | [ ] | [M12-B #57](https://github.com/kyaulabs/ngen/issues/57) | Specify one deposit/withdrawal state machine against the accepted ledger. |
| 59 | [ ] | [M12-C #58](https://github.com/kyaulabs/ngen/issues/58) | Execute one pinned chain/token adapter in an isolated test environment. |
| 60 | [ ] | [M12-D #59](https://github.com/kyaulabs/ngen/issues/59) | Validate the selected signer/custody boundary and emergency procedure. |
| 61 | [ ] | [M12-E #60](https://github.com/kyaulabs/ngen/issues/60) | Assemble independent contract/custody/legal findings into a release decision. |

**Before 57:** verify the required M0 policy/rights facts and authority for legal
work/costs. **Before 57a:** verify M7 acceptance and M12-A feasibility. **Before
58–61:** if separately selected, record the service/repo boundary and pass the
applicable legal, security and experiment gates. A no-go or deferral does not block
the game-only path. Public funds/transfers are never authorized
by a checked planning box. Record no-transfer scope or accepted selected-M12
findings before the M13 entry checkpoint.

## 62–67: Production hardening and authorized rollout

Enter after M10, full C1/C2 and selected release features pass their existing gates.
M12 is required only if external transfers ship. Freeze and name the exact release
scope; do not let optional experiments drift into it without acceptance.

| Step | Done | Issue | Outcome |
|---|---|---|---|
| 62 | [ ] | [M13-A #61](https://github.com/kyaulabs/ngen/issues/61) | Produce an installable client/server release candidate from the reviewed commit. |
| 63 | [ ] | [M13-B #62](https://github.com/kyaulabs/ngen/issues/62) | Close the production account/session and abuse-control threat model. |
| 64 | [ ] | [M13-C #63](https://github.com/kyaulabs/ngen/issues/63) | Prove recovery within the selected durability and availability objectives. |
| 65 | [ ] | [M13-D #64](https://github.com/kyaulabs/ngen/issues/64) | Establish capacity and cost for the first launch population. |
| 66 | [ ] | [M13-E #65](https://github.com/kyaulabs/ngen/issues/65) | Rehearse monitoring, staged rollout and rollback with the release candidate. |
| 66a | [ ] | [M13 handoff #135](https://github.com/kyaulabs/ngen/issues/135) | Record the MMO project's release and rollback owner, pinned engine version and transfer scope. |
| 67 | [ ] | [M13-F #66](https://github.com/kyaulabs/ngen/issues/66) | Approve the exact release scope and perform only the authorized rollout. |

**Before 67:** finish **66a** and obtain the existing independent reviews,
operating budget/coverage, explicit rollout authorization and M12 public-transfer
permission if applicable.
A plan, successful rehearsal or target population is not permission to deploy or
claim measured capacity. Close M13 only with its actual release/operations evidence.

## Progress record

Keep this short: completed step or explicit optional-selection decision, source/PR,
actual verification and remaining blocker. Update the current position above when
work advances. Do not use this log instead of the issue's complete acceptance.

| Step / decision | Source or PR | Accepted evidence / remaining blocker |
|---|---|---|
| Planning baseline | `7dfe1c2f`; live Project #4 checked 2026-11-17 | Sequence recorded against the live issue and milestone contracts. |
| 01 — C1-A | [PR #87](https://github.com/kyaulabs/ngen/pull/87), `93c7339`; [SDK guide](../creator-sdk.md) | Relocated headless and graphical consumer tests passed; validated Wayland first light passed. Push and PR checks passed on the feature tip. The [integration push](https://github.com/kyaulabs/ngen/actions/runs/44173501462) failed the merge-body line-length rule; the [manual workflow on the same SDK merge](https://github.com/kyaulabs/ngen/actions/runs/44174740056) passed 4/4 with its normal dispatch range. This does not clear the original message failure. No release or Windows SDK claim. |
| 02 — C1-B | [Issue #73](https://github.com/kyaulabs/ngen/issues/73); [starter guide](../creator-project.md) | Linux x86-64/Clang 23 Release: fresh source archive extracted, cooked, configured against an installed SDK, built, tested and run. Malformed schema, asset/build failure and non-overwriting package behavior verified in the native asset suite. Existing asset and runtime limits remain unchanged. No public binary, Windows, or general editor/MCP claim. |
| 03 — M4-A | [Issue #11](https://github.com/kyaulabs/ngen/issues/11); [ADR 0003](../adr/0003-hierarchical-coordinates.md); [precision evidence](../evidence/M4/precision-entry.md) | Approved coordinate contract exercised in six C++ cases, including 10,000 analytic round trips. Existing contact screening is separate: the 1 km single box was rejected. No live rebase, 1 km contact pass, or renderer change. |
| 04 — M4-B | [Issue #12](https://github.com/kyaulabs/ngen/issues/12); [live transfer evidence](../evidence/M4/live-origin-transfer.md) | 10,000 translations each for 10 m, 100 m and two-section 1 km hulls. Contact states matched the controls; position, tether length and velocity stayed within the approved limits. No renderer/prediction or inter-world transfer claim. |
| 05 — C1-C | [Issue #74](https://github.com/kyaulabs/ngen/issues/74); [scene authoring guide](../creator-scene.md) | Linux CLI edits, validates, saves, reloads and exports a bounded scene. Undo/redo and rejected stale, invalid and failed writes verified; the archived scene's referenced asset cooks and runs in the starter. No GUI, runtime scene interpretation, owned play/reload or MCP claim. |
| 05a — M4-F | [Issue #97](https://github.com/kyaulabs/ngen/issues/97); [particle evidence](../evidence/M4/cosmetic-particles.md) | The bounded pool followed live Jolt frames, and the synthetic GPU fixture matched fixed-origin/default HDR readbacks on Intel. The accepted flight preset and its mesh exhaust are unchanged. |
| 05b — M4-G | [Issue #98](https://github.com/kyaulabs/ngen/issues/98); [motion attachment evidence](../evidence/M4/motion-vector-attachment.md) | Opt-in Vulkan output shares mesh depth and mask tests. A live Jolt rebase matched the fixed-origin GPU bytes; all valid pixels stayed within 0.25 px. No upscaler or combined M4-C acceptance. |
| 06 — M4-C | [Issue #13](https://github.com/kyaulabs/ngen/issues/13); [combined history evidence](../evidence/M4/combined-frame-history.md) | Aligned poses and retained physical navigation across 120 live rebases. Near/far HDR, shadow and motion readbacks matched in a separate combined GPU fixture, including visible particles. No automatic origin scheduling, warp or network prediction. |
| 07 — M4-D | [Issue #14](https://github.com/kyaulabs/ngen/issues/14); [bounded warp evidence](../evidence/M4/bounded-warp.md) | A headless one-way journey crossed 36 cells and resumed real Jolt contacts only at the prepared, clear destination. Entry cancels before commitment; repeated arrivals preserve the existing body. No streaming or interactive warp. |
| 08 — C1-D | [Issue #55](https://github.com/kyaulabs/ngen/issues/55); [board playtest](../creator-play.md) | Owned Linux play/inspect/reload preserves the active snapshot on failed cooks and shares saved revision checks. Four transform edits use one cook instead of four. No project code executes. |
| 09 — C2-A | [PR #112](https://github.com/kyaulabs/ngen/pull/112); [creator MCP guide](../creator-mcp.md) | Real Linux stdio client read SDK/schema resources and metadata for one launcher-granted project. No writes, execution or listener. |
| 10 — M4-E | [PR #128](https://github.com/kyaulabs/ngen/pull/128); [flight acceptance](../evidence/M4/flight-acceptance.md) | File-backed two-system flight kept queuing frames through faults. GPU residency and corrected collision preceded arrival. Healthy and recovered runs stayed within the fixed frame gate; no production catalog or interactive warp. |
| 11 — M5-A | [PR #137](https://github.com/kyaulabs/ngen/pull/137); [local market evidence](../evidence/M5/local-market.md) | The offline buy/sell fixture conserved inventory and credits; replay, conflict, rejection and saturation passed. [Integration CPU](https://github.com/kyaulabs/ngen/actions/runs/45096798487) covered the signed merge. No persistence or integrated M5 acceptance. |
| 12 — M5-B | [PR #140](https://github.com/kyaulabs/ngen/pull/140); [bounded mining evidence](../evidence/M5/bounded-mining.md) | Concurrent and repeated claims preserved ore and cargo bounds. The explicit finite replenishment fixture conserved 13 units. See [integration CPU](https://github.com/kyaulabs/ngen/actions/runs/45119163774). No persistence or integrated M5 acceptance. |
| 13 — M5-C | [PR #143](https://github.com/kyaulabs/ngen/pull/143); [fixture combat evidence](../evidence/M5/fixture-combat.md) | Both weapons used the same first-contact damage path. All accepted projectiles resolved through saturation and shooter loss. See [integration CPU](https://github.com/kyaulabs/ngen/actions/runs/45222049044). No moving-target or integrated M5 acceptance. |
| 14 — C2-B | [PR #146](https://github.com/kyaulabs/ngen/pull/146); [validated edit evidence](../evidence/C2/validated-edits.md) | Real stdio preview and apply shared CLI operations and rejected stale, malformed and out-of-scope edits. See [integration CPU](https://github.com/kyaulabs/ngen/actions/runs/45270750062). No build/play grant or full C2 acceptance. |
| 15 — M5-D | [PR #149](https://github.com/kyaulabs/ngen/pull/149); [wreck/recovery evidence](../evidence/M5/wreck-recovery.md) | Loss and salvage kept surviving cargo with one owner under contention. Repeated recovery granted empty starters without sellable items. See [integration CPU](https://github.com/kyaulabs/ngen/actions/runs/45311770489). No durable wrecks or integrated M5 acceptance. |
| 16 — M5-E | [PR #152](https://github.com/kyaulabs/ngen/pull/152); [docking market evidence](../evidence/M5/docking-market.md) | The current docked ticket alone admitted trade. Abort and departure closed access; re-docking preserved vessel, cargo and receipts. See [integration CPU](https://github.com/kyaulabs/ngen/actions/runs/45343735796). No live contacts or integrated M5 acceptance. |
| 17 — C2-C | [PR #155](https://github.com/kyaulabs/ngen/pull/155); [owned job evidence](../evidence/C2/owned-jobs.md) | Real local stdio ran isolated cook/build/test/package. Stale and overlapping jobs were denied; cancelled work left no populated scope or archive. See [integration CPU](https://github.com/kyaulabs/ngen/actions/runs/45476150988). No playtest control or full C2 acceptance. |
| 18 — M5-F | [PR #158](https://github.com/kyaulabs/ngen/pull/158); [saved Surveyors evidence](../evidence/M5/saved-survey.md) | Both headless routes kept ore and credits conserved on loss, recovery, sale and replay. See [integration CPU](https://github.com/kyaulabs/ngen/actions/runs/45542583248). This checkpoint was headless; see the integrated gate above for current status. |
| 19 — C1-E | [PR #178](https://github.com/kyaulabs/ngen/pull/178); [sample guide](../../samples/README.md) | Both samples built and ran against an installed SDK and packaged their public sources, marker and license. The board layout passed owned play. [Integration CPU](https://github.com/kyaulabs/ngen/actions/runs/45917255343) passed 4/4. |
| 20 — C1-F | [Creator acceptance](../evidence/C1/creator-acceptance.md); [bootstrap fix #181](https://github.com/kyaulabs/ngen/pull/181) | A separate, owner-approved Pi walkthrough reproduced the modified 64 mm board from its source package and verified the corrected toolchain setup in a fresh checkout and empty Conan cache. No independent human or M5 sign-off is claimed. |
| 21 — C2-D | [PR #184](https://github.com/kyaulabs/ngen/pull/184); [owned playtest evidence](../evidence/C2/owned-playtest.md) | Separate board-only grant exercised start, inspect, reload and stop over stdio. Stale/foreign sessions were denied and teardown left the other client running. [Integration CPU](https://github.com/kyaulabs/ngen/actions/runs/46051254950) passed 4/4. No live control or full C2 acceptance. |

[board]: https://github.com/orgs/kyaulabs/projects/4/views/3
