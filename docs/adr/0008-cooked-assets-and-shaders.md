# ADR 0008: Compile versioned assets and shaders offline

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M2/M4; exact scope follows the audited repository

## Context

Runtime import, shader compilation and missing provenance would create unpredictable loading and prevent reliable releases.

## Proposed decision

Use editable sources plus manifests, a canonical cooker and versioned runtime packages. Import a declared glTF subset through fastgltf; process meshes with meshoptimizer and textures through KTX2 tooling. Compile candidate Slang shaders to validated SPIR-V with reflected layout metadata. Store content hashes, tool/compiler versions, rights, dependencies, budgets and source transforms. Runtime resolves validated handles and publishes residency only after upload completion. Snapshot/job pins keep an old resident version alive until CPU consumers finish and GPU retirement completes; generation checks alone do not provide lifetime safety.

## Alternatives

Loading arbitrary authoring formats in the runtime expands attack surface and stalls. A permanent three-language shader stack is rejected. GLSL/glslang replaces Slang if its minimum-tier capability test fails.

## Consequences

Canonical environment/settings, not arbitrary cross-platform encoders, define reproducible bytes. Importers and AI-generation SDKs stay out of shipping targets. Corrupt assets fail safely or use declared placeholders.

## Acceptance and reconsideration

Repeated canonical cooks, shader-layout/static checks, fuzzed import/package inputs, cancelled streaming and warp-prefetch tests. Revisit formats/toolchain on measured quality/performance or required extension gaps with schema migration.

## Evidence

See the [detailed specification and primary sources](../planning/renderer-core-assets.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
