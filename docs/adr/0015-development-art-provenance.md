# ADR 0015: Keep generated art in a measured development pipeline

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M2/M3/M11; exact scope follows the audited repository

## Context

4K concepts and image-to-3D tools can help authoring, but images are not production-ready geometry or a rights guarantee.

## Proposed decision

Use dated API models and bounded budgets for concept/edit references, human approval, consistent views, reconstruction comparison and Blender refinement. Keep source geometry, UV/materials, LODs, collision and cooker output as reviewed deliverables. Preserve raw outputs, prompt/model/settings, request IDs, costs, tool hashes and per-asset rights. Export only compiled assets; credentials and generation SDKs stay outside shipping targets.

## Alternatives

Accepting model output directly would bypass scale/modularity/collision/performance checks. A paid 3D service is not assumed cheaper than manual modeling. Restricted model/output licenses are not production defaults.

## Consequences

Use experimental large output sizes as a comparison option and keep a lower-resolution fallback. Preserve output bytes instead of promising deterministic regeneration. Engine licensing and asset/model rights are separate reviews.

## Acceptance and reconsideration

M2/M3 four-asset comparison measures time to accepted runtime output, including rejected drafts/cleanup. M11 verifies fleet-scale LOD/texture costs. M12/M13 rights audit blocks only affected assets until replaced/resolved.

## Evidence

See the [detailed specification and primary sources](../planning/art-pipeline.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
