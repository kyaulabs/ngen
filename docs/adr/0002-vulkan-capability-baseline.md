# ADR 0002: Use a Vulkan 1.3 capability baseline

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M0/M2; exact scope follows the audited repository

## Context

The required Iris Xe machine needs a maintained rendering path while newer GPUs can offer optional features. Advertised API versions are insufficient acceptance evidence.

## Proposed decision

Require Vulkan 1.3 with explicitly queried/enabled dynamic rendering, synchronization2, timeline semaphores, supported render/depth formats and graphics/presentation queues. Use ordinary material descriptors first. Enable Vulkan 1.4, descriptor indexing, buffer device address, indirect optimizations and RT only through feature-specific profiles with a forced-basic test mode. Do not require GPU FP64. Below-baseline support is a separate product decision, not an implicit Vulkan 1.2 fallback.

## Alternatives

Requiring all modern features would exclude the intended minimum. Maintaining multiple API backends or a 1.2 extension path now creates unrequested test/configuration work.

## Consequences

The basic path remains the compatibility contract. Optional feature paths carry extra test cost. A missing required feature produces a useful compatibility report rather than an unsupported rendering attempt.

## Acceptance and reconsideration

B0 captures the actual physical device/WSI and tests missing optional features; M2 resource/synchronization validation passes. Accept the baseline only after the raw profile and first-light result exist.

## Evidence

See the [detailed specification and primary sources](../planning/renderer-core-assets.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
