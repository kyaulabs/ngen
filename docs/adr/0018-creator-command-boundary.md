# ADR 0018: Share validated creator operations across human and MCP tools

- Status: Proposed
- Date: 2026-11-17
- Decision owner: Sean Bruen
- Adoption gate: C1-A/B and C2-A; roadmap direction selected, implementation unverified

## Context

NGEN currently has engine libraries, asset/shader tools and a development inspector,
not a consumable creator SDK. The owner now requires a reusable 3D-game workflow
and a local MCP server for AI-assisted authoring, builds and owned playtests.
Duplicating edits in CLI, editor and MCP would allow their validation and ownership
rules to diverge. Linking creator/AI services into every game would break offline
and headless separation.

## Proposed decision

Provide the smallest project/scene/build/playtest operations required by C1's real
consumer. Human CLI/editor and the local MCP adapter use the same validated
operations, typed errors and explicit project/revision/job/session identities.
They do not write active snapshots or flagship authority directly. Keep these
creator services outside runtime dependency graphs that do not select them.

The generic engine has no mandatory policing, sovereignty, MMO economy or AI-model
rules. The flagship and other games supply those policies. An MCP request is
untrusted input, not authorization; enforce workspace/capability/revision checks
at the operation boundary. Source generation/build/play execution needs explicit
authority and demonstrated host isolation, not a general shell exposed by default.

## Alternatives considered

- Independent CLI/editor/MCP mutation paths duplicate safety and recovery rules.
- A universal command bus/plugin ABI before the second consumer adds unsupported
  extension promises; use existing concrete operations before extracting an API.
- Exposing an unrestricted console/filesystem or live shard to MCP makes project
  creation authority equivalent to host or production administration.
- Embedding MCP/AI into all games prevents a minimal offline/headless consumer.

## Consequences

A real external project and non-space sample drive API scope. Creator tooling is
first-class, while optional graphics stay optional. Local authoring and code
execution are distinct grants. Additional API/version/isolation work is required;
no existing inspector test establishes SDK or MCP readiness. Remote collaboration
requires a separate design. Current AGPL/content rights and first-party-server
scripting restrictions remain unchanged.

## Acceptance and reconsideration

C1 proves new-project creation, authored scenes, diagnostics and packaged samples
without engine-core edits. C2 tests the same outcomes through actual MCP messages
and bounded owned jobs, including malicious input and denied capabilities. Record
host/isolation/protocol versions and limitations. This proposal does not ratify
other ADRs or claim those tests ran. Revisit for a demonstrated second integration,
remote users or a requested new execution/extension boundary.

## C1-A review scope

The first external consumer uses an opt-in CMake install of static engine
libraries and an explicit public header set. The default package is headless;
rendering is a separate component. This reuses the current C++ targets without
committing to a stable binary ABI, game-specific content or any new creator
operation. The [SDK guide](../creator-sdk.md) describes this boundary. Review of
this slice does not ratify C1-B/C2-A operations or grant execution capabilities.
The broader proposal remains open.

## C1-B review scope

The [standalone starter](../creator-project.md) has a bounded versioned
manifest, fixed source layout, and non-overwriting source packaging. Its command
line validates files without executing project code. Humans invoke the existing
cooker and CMake/CTest directly in an owned workspace; project metadata does not
select executables, arguments, or host roots. This is not a sandbox or a grant
for an MCP client to invoke a build. No editor API or new execution service is
introduced.

## C1-C review scope

The Linux project CLI uses one [scene authoring boundary](../creator-scene.md)
for bounded edits and explicit revision checks. A same-directory atomic save
publishes the current scene with its undo/redo state. Source packages contain
only the active scene and admitted references, not local history. No editor,
active runtime snapshot mutation, MCP capability or execution service is added.

## C1-D review scope

The [owned board playtest](../creator-play.md) starts trusted tools in Linux
namespaces with staged data only. It does not build or execute project code.
The CLI uses the saved scene revision and its own play generation. Native
publication follows a checked cook and a deterministic simulation tick.
No MCP grant, arbitrary execution, GUI editor, or broader status change.

## C2-A review scope

The optional [creator MCP companion](../creator-mcp.md) pins the official
Python SDK 2.3.0 and uses Linux stdio. The existing creator operations already
run in Python; this avoids duplicating validation or implementing a protocol
stack. The launcher grants one directory before the session starts. The client
can discover its handle and request validated metadata, but cannot grant
another root, read files by path, write, or execute project code. No network
transport or engine dependency is added. This slice does not ratify the broader
proposal or authorize later C2 capabilities.

## C2-B review scope

The [creator MCP companion](../creator-mcp.md) stays read-only unless the
launcher grants edits to its one opened project. MCP and the CLI share bounded
manifest rename, fixed `main.cpp` replacement, and scene edit operations. Preview
returns a structural diff; a save rechecks the expected revision and atomically
replaces one file. Source is text only at this boundary, not compiled or executed.
The grant does not include arbitrary paths, asset writes, live snapshots, new
roots, or build/play. No other ADR is ratified by this slice.

## C2-C review scope

A separate [local job grant](../creator-jobs.md) starts one fixed offline job
at a time for a captured project revision. The Linux worker verifies its cgroup
limits before the owner releases it into Bubblewrap; it cannot inherit the
launcher environment or use host sockets. Cancellation stops and checks the
owned cgroup before reporting completion. Only the source packager publishes a
local archive, and it never runs project code. There is no dependency download,
public release, shell access or playtest authority. The broader ADR is still
Proposed.

## Evidence

[Creator scope and acceptance](../planning/creator-toolchain.md), the current
`engine/tools/inspector.hpp` queued-command contract and root CMake target graph.
The owner selected a reusable 3D foundation and local authoring plus playtests;
these are planning requirements, not implementation acceptance.
