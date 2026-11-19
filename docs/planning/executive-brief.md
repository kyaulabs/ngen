# NGEN executive brief

NGEN has reached its first product gate: a polished offline spacecraft flight
demonstration. M3 was accepted and normally merged through [PR #10] into `develop`
at `03b76c724a556a30944b39072afbee4aa2dfcf8b`, following accepted M2 at `4b554cc`.
Exact [post-merge CI] passed all four jobs; merge signature/ancestry verify.
[0.1.0] is now published as source only at reviewed main `2d1a0539`; public
source/checksums, release CI and the completed back-merge verify. It does not
include the private playable scene/bundles/media. The source-only
[0.6.0 release](../releases/0.6.0.md) adds bounded headless warp with collision
checks before arrival. Owned play, inspection and reload are next in the
[execution order](execution-order.md); streaming, local MCP and multiplayer
remain future work in the [roadmap].

The working C++20 foundation uses pinned Conan/CMake/Ninja graphs, explicit RAII
ownership, bounded commands/jobs and immutable owning snapshots. SDL/Vulkan,
VMA, offline Slang shaders, cooked assets, PBR/IBL/shadows and the inspector provide
the presentation path. Double-position Jolt and one shared flight controller
support tactical/direct input, authored cameras and physical RCS. Presentation
and original audio consume published actuation rather than inventing thrust.
One controlled hull and 100 asteroids are deliberate scope, not a galaxy claim.

The owner accepted handling, audio, genuine disconnected startup/listening, and
the final Visual Identity 1.4 preview/private clip. Supplied commercial typefaces
remain separately licensed; existing AGPL/dependency obligations remain intact.
Native tests, strict sanitizer consumers, scoped source reviews and hosted CI
support the delivered paths. Windows/laptop timing and sampled memory evidence
retain their actual revisions, hardware and driver caveats; they are not
relabelled as the later UI. The initial recording's 3,854-call observation remains
untraced/non-benchmark. The unchanged full GUI suite passes on an owned isolated
Wayland display, while earlier active-desktop failures remain retained.
See [final verification] and [delivery] for exact scopes and artifact identities.

Keep the delivered limits: 249,942 triangles within the 250,000 budget, 247 ship
instances under the existing admission boundary, Context ownership within
512 MiB, owned GPU resources within 1 GiB, and process memory within the declared
6 GiB gate. Counters, resource categories and sampled peaks are not interchangeable.
Do not weaken allocation, validation, sanitizer or capacity checks to extend scope.

The engine goal now explicitly includes a usable creator workflow: consumable
SDK, project/build/asset operations, scene tools, documentation and space/non-space
samples without engine-core edits. These are mandatory C1/C2 deliverables, not an
optional graphics experiment. They can ship before MMO production and do not make
AI, databases or flagship rules mandatory dependencies of every game. The first
MCP companion is local authoring plus authorized build/test jobs and owned
playtests; no unrestricted host shell, credentials or live-MMO administration.
See the [creator plan].

The flagship delivery sequence remains:

1. **M4:** accept the coordinate/precision decision and bounded precision evidence;
   then canonical cell/offset coordinates, tick-barrier local-frame transfer,
   coherent render history, committed warp and two-system residency/prefetch.
   Security manifests establish protected starts and a greatly larger lawless
   region. C1-A/B's consumable creator baseline proceeds alongside M4.
2. **M5:** finish a small offline game: conserved inventory, mining, two weapon
   types, loss/wreck/recovery, docking/trading and one saved first-party quest.
3. **M6–M7:** make that same simulation authoritative, then add crash-safe
   persistence, markets, industry and bounded NPC/faction progression.
4. **M8–M10:** implement security law, NPC enforcement and lawless sovereignty
   before measuring fleet/time-dilation limits and fenced regional handoff.
   Unprovoked protected-space aggression is criminal; lawful defense/explicitly
   legal combat remain possible. Police punish rather than grant invulnerability.
   M7/M8 preserve titles/resource rights over stations, planets, moons and sectors;
   protected starter jurisdiction stays NPC-controlled. Lawless combat does not
   waive economic integrity or server authority.
5. **M13:** release only a measured, reviewed scope with recovery, security,
   operations and a separately authorized rollout.

M11 graphics are selected individually for measured benefit, not a mandatory
renderer rewrite. Former M11-F editor work belongs to mandatory C1, retaining
issue #55; it is not duplicated. M12 external settlement is independently optional: legal,
custody, security and explicit implementation/public-transfer authorization remain
gates. Its failure or rejection must not block a game-only release. Lua, network
transport, PostgreSQL, astronomical travel and distributed authority are future
work, not delivered features. Fleet counts and five-figure concurrency remain
fixtures or aspirations until measured; no dates, staffing or capacity promise is
inferred from this roadmap.

The dedicated [NGEN kanban] now contains 67 remaining issues and 67 unique Todo
cards across twelve open milestones, verified after explicitly authorized sync.
Eleven issues and two milestones were added; 25 existing bodies were updated,
including the retained editor issue #55 as C1-D. Original issue/card IDs, labels,
assignees and states were preserved. The [tracker index] links actual records to
the immutable planning source. Milestones remain undated and issues unassigned,
with no implementation, spending, infrastructure or deployment grant.
No M4, policing/ownership, SDK or MCP code starts through this documentation. Historical M0/M1 evidence and individual
ADR statuses are not silently ratified by roadmap maintenance.

[0.1.0]: https://github.com/kyaulabs/ngen/releases/tag/v0.1.0
[creator plan]: creator-toolchain.md
[PR #10]: https://github.com/kyaulabs/ngen/pull/10
[roadmap]: milestones.md
[final verification]: ../evidence/M3/ui/final-verification.md
[delivery]: ../evidence/M3/package/final-delivery.md
[post-merge CI]: https://github.com/kyaulabs/ngen/actions/runs/43961359680
[NGEN kanban]: https://github.com/orgs/kyaulabs/projects/4/views/3
[tracker index]: github-tracker.md
