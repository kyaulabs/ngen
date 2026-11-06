# ADR 0013: Use bounded first-party gameplay scripts

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M5; exact scope follows the audited repository

## Context

Quests and faction decisions benefit from authoring flexibility, but modding is excluded and state invariants must remain protected.

## Proposed decision

Use pinned Lua 5.5.1 as the provisional VM for trusted compiled quest/faction content. C++ owns physics, network validation, inventory and accounting. Scripts read snapshots and enqueue narrow validated commands. Cap VM memory, instruction work and native call duration; expose controlled clocks/RNG. Persist explicit quest data with versions, not VM stacks. Select and test a C++-safe error strategy before writing bindings.

## Alternatives

Hardcoding every quest slows content changes. Unrestricted script pointers, OS APIs or player uploads conflict with the scope. Luau is an alternative only if authoring benefits outweigh an extra compatibility stream.

## Consequences

Interpreter quotas do not interrupt a blocking native call. Script failures cannot leave partial economic effects. Lua compiler/build/bytecode mode must agree between cooker and runtime.

## Acceptance and reconsideration

M5 runaway-loop, allocation-failure, native-error and malformed-bytecode tests prove bounded failure and RAII cleanup. Save migration tests cover script version changes. Reassess only when concrete authoring needs exceed the chosen VM.

## Evidence

See the [detailed specification and primary sources](../planning/simulation-network-persistence.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
