# ADR 0003: Use hierarchical coordinates and frame epochs

- Status: Accepted (M4-A coordinate contract only)
- Proposed: 2026-11-02
- Accepted: 2026-11-18, by Sean Bruen
- Scope: canonical coordinates and analytic transforms; live domain transfers and rendering remain open

## Context

Continuous interstellar positions, close docking and an FP32 graphics baseline cannot safely share one absolute float position.

## Decision

Store a position as three signed 64-bit cells and three binary64 offsets in
`[-2^31, 2^31) m`, with a cell width of `2^32 m`. Normalize with a checked
integer carry; reject nonfinite offsets or an unrepresentable carry or cell.
Keep long-range positions split rather than flattening them to one double.

At one simulation tick, transform a sample between nonrotating frames by
adding source-minus-target origin displacement to its local position and
source-minus-target origin velocity to its velocity. Check frame ID, epoch
and tick, then stamp the result with the destination. Check cell adjacency
before subtraction. Reject nonfinite input/result or any component of local
input, origin displacement or local result outside `[-2^20, 2^20] m`.
This is a coordinate API bound, not a physics or GPU precision limit.

A future live transfer must update physics, queries, current/previous
presentation and prediction state together at a tick boundary. This decision
does not implement or accept that transfer.

## Alternatives

A single galaxy-scale double eventually loses local resolution and invites premature float conversion. GPU FP64 violates the baseline. Arbitrarily rotating physics frames add transformations/fictitious forces not needed for fixed celestials.

## Consequences

Coordinate operations require explicit frame identity and checked bounds.
Far-field rendering needs a separate scale-aware representation; the cell
size is not a gameplay unit. The math API does not add shapes, change World
admission, or expose a wire or persistence schema.

## Acceptance and reconsideration

M4-A tests canonicalization, overflow/rejection, analytic position residual
≤1 mm and velocity residual ≤0.1 mm/s in declared fixtures. M4-B still needs
agreed contact tolerances and live transfers, including a representative 1 km
hull without silently raising the box cap. M4-C still needs presentation and
prediction history evidence. Revisit the cell size if measured streaming or
network ranges justify it, preserving any future persisted schema.

## Evidence

See [entry evidence](../evidence/M4/precision-entry.md) and the [detailed
specification and sources](../planning/simulation-network-persistence.md).
Owner approval applies to the scoped contract and entry model, not a live
rebase, contact, renderer or milestone verdict.
