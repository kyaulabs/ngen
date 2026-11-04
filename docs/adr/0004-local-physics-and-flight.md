# ADR 0004: Separate astronomical travel from local contacts

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M3/M4; exact scope follows the audited repository

## Context

The game combines close collision, large hulls, assisted direct flight and AU-scale warp. A rigid-body world is not the canonical galaxy representation.

## Proposed decision

Use Jolt for bounded local contacts and queries, with a CPU-only double-position configuration. Put interacting bodies in one contact domain. Both direct and tactical modes submit intent to the same force/torque model and preserve motion when switching. Represent ship equipment as logical components/hit regions unless a visible detached body is needed. Warp follows a committed game trajectory and exits only when destination state/content/authority is ready.

## Alternatives

Integrating high-speed interstellar rigid bodies wastes work and weakens collision fidelity. A rigid body for every module multiplies solver cost. A custom physics engine is rejected until a focused Jolt/alternative experiment shows a necessary gap.

## Consequences

Large capitals may need simpler collision proxies and explicit supported velocity/size limits. Warp is a distinct game mode. Domain migration must preserve contact groups; ghost queries never create authoritative impulses.

## Acceptance and reconsideration

B3 tests 10m/100m/1km hulls, docking, contact density, relative velocity and 10,000 frame changes. Verify headless link graph. Choose PhysX/Bullet or a restricted collision representation only from failed requirements with evidence.

## Evidence

See the [detailed specification and primary sources](../planning/simulation-network-persistence.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
