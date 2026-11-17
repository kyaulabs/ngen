# ADR 0014: Preserve repository policy in builds and reviews

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M0/M1; exact scope follows the audited repository

## Context

The repository has specific identity, signing, Git Flow and review requirements. Reproducibility and isolated CI must reinforce those rules.

## Proposed decision

Use audited current repository patterns, target-scoped CMake/Conan profiles and locks, pinned tools and dependency provenance. Separate correctness/performance lanes and trusted/untrusted runner contexts. Remove short-lived checkout credentials before executing PR code. Keep YubiKey/GPG signing with the authorized developer. Independent AI review precedes formal kyau review; bot PR creation/merge uses the actual template and completed Test Plan. Prove post-merge signatures/identity compliance for the selected merge strategy.

## Alternatives

Bypassing hooks/signatures for convenience is prohibited. A persistent personal desktop runner for arbitrary PR code is rejected. An action badge or local ruleset JSON does not establish enforcement.

## Consequences

Unavailable signing or repository access blocks the affected operation, not useful local reviewable work. No branches/commits/services are authorized by this planning turn. CI failures are fixed and relevant checks rerun with new evidence.

## Acceptance and reconsideration

M0 policy comparison and merge-semantics check; M1 clean build/test proof; isolated runner threat model before provisioning; milestone evidence tied to exact revision. Review template/hook changes separately from engine changes.

## M1 scope accepted by the implementation request

The 2026-11-04 request approves target-scoped C++20, reproducible native build preparation and meaningful verification while retaining current delivery policy. Conan 2 profiles/lock and shared CMake/Ninja presets now cover the Linux implementation. Windows support is not claimed. Independent review, hosted workflow results and signature-preserving PR delivery remain gates, not inferred approvals. The earlier planning-only restriction is superseded for M1 by that request. See [M1 evidence](../evidence/M1/README.md).

## Evidence

See the [detailed specification and primary sources](../planning/build-review-infrastructure.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
