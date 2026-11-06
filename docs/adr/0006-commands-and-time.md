# ADR 0006: Make commands and time explicit

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M1/M3/M6; exact scope follows the audited repository

## Context

Offline play, prediction, replay testing and eventual regional time dilation need stable command semantics without an early network implementation.

## Proposed decision

Use typed ordered commands with identity/sequence, simulation ticks and bounded results/events. Offline session authority and server authority share game semantics. Start flight physics at 60 steps/s; evaluate a 30Hz authoritative game tick with two physics substeps. Rendering interpolates immutable snapshots. Use separate simulation, monotonic wall and durable-economy clock types. Control leases and gameplay permissions are versioned server facts.

## Alternatives

Frame-delta gameplay ties rules to GPU speed. Direct UI/ECS mutation bypasses validation. Cross-platform whole-world deterministic lockstep is not the networking design; deterministic fixtures are narrower and useful.

## Consequences

Fixed-step rates remain proposed until flight/contact tests pass. AI/strategic planning can run less frequently. Command persistence is selective; no event-sourced copy of every rendered frame is required.

## Acceptance and reconsideration

M1 repeated command fixture, M3 mode-switch/momentum tests, M6 reconciliation and stale-control rejection. Clock-jump/restart/time-dilation tests prove each domain uses its declared clock. Revisit rates only with recorded latency/fidelity/cost evidence.

## M1 scope accepted by the implementation request

The 2026-11-04 request approves fixed simulation ticks, ordered value commands and immutable published snapshots. The fixture uses a nominal 60 ticks/second; explicit `step()` calls alone advance it. Commands must target the next tick and have strictly increasing nonzero sequence numbers. A tick applies its batch transactionally; arithmetic overflow discards that batch without advancing state or the clock, and consumed sequences cannot be replayed. Publication is a bounded value copy with read-only accessors; consumers retain the copy, not a borrowed span into a destroyed snapshot. This is not a concurrent shared-snapshot channel. Unit conversion truncates fractional millimetres toward zero and rejects nonfinite/out-of-range input. Flight rates, durable clocks and network semantics remain proposed. See [M1 evidence](../evidence/M1/README.md).

## Evidence

See the [detailed specification and primary sources](../planning/simulation-network-persistence.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
