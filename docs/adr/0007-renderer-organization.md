# ADR 0007: Start with fixed PBR passes and measured Forward+

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M2/M3/M11; exact scope follows the audited repository

## Context

The first useful renderer must produce an attractive flight scene on integrated graphics without an early general rendering framework.

## Proposed decision

Use a fixed shadow/opaque-PBR/transparent/post/UI pass schedule with explicit resource-use/barrier tables, one graphics queue and two frame contexts. Choose metallic-roughness materials and capped forward lighting initially. Move to clustered Forward+ when light counts justify it. Ordinary GPU resources retire after their last submission; presentation resources need separately proven presentation retirement, preferably maintenance1 present fences. Per-swapchain-image present-wait semaphore reuse follows image reacquisition, not a frame fence. Camera-relative transforms and temporal history have explicit frame epochs. Use reversed-Z where the selected depth format supports the tested path.

## Alternatives

Deferred shading may help some scenes but is not assumed faster on Iris Xe. A bespoke universal render graph or async compute scheduler is premature. Evaluate AMD RPS before writing a graph once real pass complexity warrants one.

## Consequences

Graph sophistication and vendor effects cannot block first flight. CPU and GPU allocation ownership are separately visible. Celestial rendering and local geometry share a defined occlusion/composition contract.

## Acceptance and reconsideration

B0 lifetime/synchronization/resize tests, B1/B2 fixed-scene timing and visual review, B3 history stability. Add graph/indirect/RT paths only through measured before/after gates that retain the basic renderer.

## Evidence

See the [detailed specification and primary sources](../planning/renderer-core-assets.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
