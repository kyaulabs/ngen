# NGEN executive brief

NGEN has reached its first product gate: a polished offline spacecraft flight
demonstration. M3 was accepted and normally merged through [PR #10] into `develop`
at `03b76c724a556a30944b39072afbee4aa2dfcf8b`, following accepted M2 at `4b554cc`.
The next workstream is M4 coordinates, local-frame transfer and travel/streaming,
not multiplayer or a larger editor. The [roadmap] defines its tasks and gates.

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

The delivery sequence remains:

1. **M4:** accept the coordinate/precision decision and bounded precision evidence;
   then canonical cell/offset coordinates, tick-barrier local-frame transfer,
   coherent render history, committed warp and two-system residency/prefetch.
2. **M5:** finish a small offline game: conserved inventory, mining, two weapon
   types, loss/wreck/recovery, docking/trading and one saved first-party quest.
3. **M6–M7:** make that same simulation authoritative, then add crash-safe
   persistence, markets, industry and bounded NPC/faction progression.
4. **M8–M10:** enforce event permissions before measuring fleet/time-dilation
   limits and introducing single-writer fenced regional handoff.
5. **M13:** release only a measured, reviewed scope with recovery, security,
   operations and a separately authorized rollout.

M11 graphics/tools are selected individually for measured benefit, not a mandatory
renderer rewrite. M12 external settlement is independently optional: legal,
custody, security and explicit implementation/public-transfer authorization remain
gates. Its failure or rejection must not block a game-only release. Lua, network
transport, PostgreSQL, astronomical travel and distributed authority are future
work, not delivered features. Fleet counts and five-figure concurrency remain
fixtures or aspirations until measured; no dates, staffing or capacity promise is
inferred from this roadmap.

The owner requested a dedicated NGEN organization Project and all remaining
M4–M13 atomic task issues. Milestones preserve completion dependencies; issue
bodies carry acceptance evidence and optional/release gates. Creating a card does
not authorize implementation, spending, infrastructure or deployment. No M4 code
is started by this planning/tracker update. Historical M0/M1 evidence and individual
ADR statuses are not silently ratified by roadmap maintenance.

[PR #10]: https://github.com/kyaulabs/ngen/pull/10
[roadmap]: milestones.md
[final verification]: ../evidence/M3/ui/final-verification.md
[delivery]: ../evidence/M3/package/final-delivery.md
