# ADR 0010: Commit economic effects transactionally

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M5/M7; exact scope follows the audited repository

## Context

Concurrent trades, rewards, destruction and offline manufacturing must survive retries and crashes without duplication.

## Proposed decision

At M5 use a local transaction/save adapter with the same economic invariants. Introduce PostgreSQL at M7 for durable online state, with a worker boundary for database I/O, unique command IDs/digests, stored results and a transactional outbox. Inventory, money, fees, reservations and order state commit together. Use checked integer subunits and immutable balanced postings. Use row locks/consistent lock order or SERIALIZABLE with bounded whole-transaction retries, with a documented invariant proof. Acknowledge durable success only after commit.

## Alternatives

Updating cached balances then asynchronously saving invites loss/duplication. Per-frame SQL blocks simulation. Exactly-once transport is not assumed; at-least-once delivery plus durable idempotency provides one business effect.

## Consequences

Database outages suspend value-changing actions. Destruction can enter a locked pending-durability state; loot/control/resale must not escape that state before its operation resolves. Checkpoint poses and acknowledged economics have distinct RPOs.

## Acceptance and reconsideration

B6 generated concurrent sequences, crash-at-boundary tests, duplicate/reordered outbox delivery, order cancel/fill and death/salvage races, backup/PITR restoration. Adopt stronger replication only after its durability/fencing assumptions are proven.

## Evidence

See the [detailed specification and primary sources](../planning/simulation-network-persistence.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
