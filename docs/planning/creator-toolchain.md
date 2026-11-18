# Creator toolchain and local MCP

Status: owner-selected roadmap direction, not implemented. NGEN 0.1.0 is a
source-only engine/flight foundation, not an installable creator SDK, scene editor
or MCP server. C1 and C2 run alongside the flagship roadmap; they are required
creator deliverables, not optional M11 graphics experiments. No new dependency,
paid AI access, hosted service or engine implementation is authorized by this plan.

## Goal and boundary

A programmer with game-development knowledge can create a new 3D game without
editing NGEN internals or borrowing its private flight scene. Prove the workflow
with a small non-space game as well as a legally redistributable space example.
This is a reusable 3D foundation, not a promise of every genre or feature parity
with an established editor suite.

The engine owns reusable rendering, assets, physics, input/audio, simulation and
authoring contracts. The flagship owns flight assists, security law, police,
property/sovereignty, factions and economy rules. A project chooses its adapters;
offline games do not acquire MMO, database, settlement or MCP dependencies.
Creator-authored source is their game's code, not permission for players to upload
arbitrary code into the flagship server. Existing first-party scripting limits
remain until separately reconsidered.

Current evidence supports libraries, an asset/shader pipeline and the bounded
inspector's queued generation-checked commands. The C1-A
[source-built SDK](../creator-sdk.md) exposes a minimal install boundary. C1-B
adds one [standalone starter workflow](../creator-project.md), not a general scene
editor or an execution service. Reuse those implementations
where appropriate; do not invent a universal plugin ABI, reflection framework or
second simulation owner before an actual consumer requires one. See
[ADR 0018](../adr/0018-creator-command-boundary.md).

## C1: usable creator workflow

1. Audit and publish the minimal supported SDK/dependency boundary. A separate
   consumer builds against declared targets/headers without private includes,
   graphics in its headless graph or mandatory flagship content/services.
2. Provide a versioned project manifest/template and a supported path through
   create, validate, import/cook, configure, build, test, run and package. Commands
   report structured diagnostics and meaningful exit codes; existing tools come
   before new wrappers. Future CLI names are not public APIs yet.
3. Provide enough scene authoring to create/save/load a real sample: entity and
   component properties, transforms, cameras, lights, materials and asset references.
   Validate imports and references; offer undo/redo and atomic versioned saves.
4. Connect play/stop, inspection and bounded reload to the same validated command
   boundary. Measure one real authoring bottleneck before/after. Stale generations,
   failed cooks and invalid edits cannot corrupt an active snapshot or saved project.
5. Supply redistributable samples, API/task-oriented documentation, debugging and
   packaging instructions. A creator should not need private fonts/content rights
   or unpublished build knowledge to follow the sample.
6. Observe a developer create, modify, test and package the non-space sample from
   a fresh supported setup, without engine-core edits. Record usability failures,
   dependency/asset notices and actual platform results—not an unsupported ABI,
   clean-machine or commercial-licensing claim.

C1-A/B establish a consumable baseline before M5 expands flagship gameplay. They
can proceed alongside M4's independent precision gate. Full C1 acceptance remains
a prerequisite for the production engine/creator release; it does not reopen M3.

## C2: AI-facing local MCP

The first server is a local creator companion, preferably stdio, not a public
network listener or a live-MMO administration endpoint. Select a maintained SDK,
protocol version and implementation language from actual compatibility evidence
at C2-A; the engine does not gain an AI model/runtime dependency.

| Capability | Permitted first-version work | Boundary |
|---|---|---|
| Discover/read | Supported SDK docs, project schema, scene/asset metadata and bounded diagnostics | Granted project roots only; no credentials, hidden unrelated sessions or arbitrary host-file reads |
| Author | Create project/source files and apply validated scene/content changes | Explicit writable workspace; canonical paths, type/size bounds, expected revisions, diffs and atomic save/rollback |
| Build/test | Start approved configure/build/cook/test/package jobs | An explicit execution grant, allowlisted argument arrays, owned workers, deadlines/concurrency/output bounds and a constrained environment |
| Playtest | Start/stop an owned development runtime, inspect snapshots and apply supported development commands | Explicit session/authority/generation handles; never another user's process, a live production shard or direct economic-state writes |

CLI, GUI and MCP are adapters to creator operations, not competing implementations
of validation. AI requests are untrusted input. Authenticate/authorize the actor
and operation independently of resource/session handles. Schema validation,
capability checks and stale-revision checks happen on every mutation; tool
annotations and model instructions are not a security boundary.

Project source, compiler/cooker inputs and generated programs can execute code.
Do not call them safe because the tool name says build or play. Require a separate
project execution grant and an isolation design proven on each supported host;
do not silently fall back to unrestricted execution when isolation is unavailable.
Keep host credentials out of child environments and logs. Dependency downloads,
external network, package installation, spending and publication require separate
capabilities; they are not implicit authoring permission. A general shell tool,
host-wide filesystem tool and production administration are out of scope.

A mutation returns actionable structured success/error information and the resulting
revision or job/session handle. Jobs have bounded lifetimes and cancellation;
cleanup reaps only owned descendants. Recovery cannot publish partial projects or
claim success after cancellation. Human review remains available for diffs and
sensitive actions; ordinary previously granted work need not ask for approval at
every command. MCP never supplies an engine-specific API key or bypasses a client
host's own authorization policy.

C2 acceptance is a real protocol-level sequence from discovery to creating/editing,
building/testing and playtesting a small game using C1. A scripted MCP client can
prove protocol/operation behavior without paid model calls; separately record any
actual AI-client usability evidence. Exercise malformed/oversized requests, stale
handles, path/symlink escape, credential access, prompt-shaped asset text, failed
builds, output floods and cancellation. No cross-project/production mutation,
secret disclosure or orphaned owned process is permitted. Do not claim a complete
sandbox or security audit from mocks alone.

## Open implementation decisions

Select exact public API/manifest versions, editor packaging, supported SDK/transport
version, host isolation and measured resource limits during the corresponding entry
experiments. No new framework, service mesh, account system, broad plugin ABI or
visual scripting language is selected here. Remote/multi-user MCP is a later,
separately designed trust boundary—not part of C2's local acceptance.

The [MCP tools specification, revision 2026-07-28](https://modelcontextprotocol.io/specification/2026-07-28/server/tools)
was reviewed as protocol evidence. Its tool schemas, explicit state handles,
structured errors and access-control/rate-limit guidance do not supply NGEN's
project authorization or execution isolation. Pin and test the actual selected
protocol/SDK at implementation time.
