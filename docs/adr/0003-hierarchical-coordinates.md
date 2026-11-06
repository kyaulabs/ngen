# ADR 0003: Use hierarchical coordinates and frame epochs

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M1/M4; exact scope follows the audited repository

## Context

Continuous interstellar positions, close docking and an FP32 graphics baseline cannot safely share one absolute float position.

## Proposed decision

Propose GalacticPosition as three checked signed 64-bit cell indices and three bounded CPU-double offsets, with cell length 2^32 meters and offsets in [-L/2,L/2). Normalize at serialization and arithmetic boundaries. Compute bounded relative displacement before conversion to GPU floats. Physics frames initially translate without rotating; frame epoch and origin velocity accompany poses. A frame change updates current/previous presentation, prediction, queries and physics state at one tick boundary.

## Alternatives

A single galaxy-scale double eventually loses local resolution and invites premature float conversion. GPU FP64 violates the baseline. Arbitrarily rotating physics frames add transformations/fictitious forces not needed for fixed celestials.

## Consequences

Coordinate operations require explicit frame identity and checked bounds. Rendering distant celestials uses a separate scale-aware representation. The cell size is a provisional engineering constant, not a gameplay unit.

## Acceptance and reconsideration

B3 analytic residual, large-cell subtraction, normalization/overflow, rebases and shadow/motion-history tests. Revisit cell size if streaming/network ranges provide measured evidence, preserving persisted schema compatibility.

## Evidence

See the [detailed specification and primary sources](../planning/simulation-network-persistence.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
