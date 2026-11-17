# ADR 0009: Use authoritative snapshots and local flight prediction

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M6; exact scope follows the audited repository

## Context

Manual piloting needs responsive feedback, while inventories, combat and control ownership require server authority.

## Proposed decision

Use GNS as a bounded transport adapter. Add account/server authentication, protocol versioning, input sequences, control versions, snapshot/frame/authority epochs, baselines and per-client budgets. Predict the controlled ship using the flight model and reconcile to authoritative snapshots. Schedule relevant state by spatial/sensor/ownership/combat interest. Separate unreliable simulation updates from reliable lifecycle/receipt messages. Bulk asset transfer cannot block gameplay.

## Alternatives

Client authority fails economic/combat integrity. Whole-world physics lockstep creates unnecessary determinism/latency obligations. MsQuic remains an alternative if GNS fails a measured requirement, not a simultaneous production transport.

## Consequences

Encryption does not identify a game account by itself. The project owns replication and denial-of-service limits. Visual control switches may be immediate while authoritative handover waits for validation.

## Acceptance and reconsideration

B5 32-client adverse-network trace, malformed packet corpus, reconnect/replay tests and bandwidth measurements. Increase CCU only after bounded memory/queues and latency tails pass. Add lag-compensated weapons only with an explicit abuse/tolerance policy.

## Evidence

See the [detailed specification and primary sources](../planning/simulation-network-persistence.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
