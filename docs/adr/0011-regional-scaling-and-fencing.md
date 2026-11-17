# ADR 0011: Scale regions with explicit clocks and fenced ownership

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M9/M10; exact scope follows the audited repository

## Context

Large battles and a shared galaxy require load management and later ownership movement without two authoritative writers.

## Proposed decision

Keep one authoritative process through initial multiplayer. Regional time dilation advances fixed simulation ticks less often per wall second; combat-affecting systems share that clock. Interacting contact groups stay together. At M10, prepare destination, quiesce source at end tick T, resolve/record in-flight operations, capture T and its pending operations, verify tick/schema/hash and durably acknowledge the recoverable transfer payload, then atomically transfer owner/epoch with a reference to that payload and resume destination at T+1. At most one valid writer exists for an entity across all epochs at any instant. Every protected recipient validates the fencing epoch atomically with its mutation.

## Alternatives

Lease expiry alone cannot stop a paused old writer. Copying a snapshot while the source keeps advancing loses intervening state. Splitting a dense interacting battle among machines is not automatically safe or economical.

## Consequences

Integrity may require a visible pause under failure. Migration recovery differs before/after ownership commit. Global CCU scales across independent regions; dense battle capacity remains bounded by a declared fixture and admission policy.

## Acceptance and reconsideration

B7 closed-fixture equivalence with identical tick inputs/RNG and controlled external events, plus separate wall-time economy fairness and work/wall-second evidence; B8 10,000 transfers with paused writers, partitions and every crash boundary. New topology requires measured cost/latency and restore evidence, not an aspirational player count.

## Evidence

See the [detailed specification and primary sources](../planning/simulation-network-persistence.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
