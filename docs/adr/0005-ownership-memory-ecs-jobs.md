# ADR 0005: Keep ownership and scheduling explicit

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M1/M3; exact scope follows the audited repository

## Context

Predictable hot paths and C++ lifetime safety require clear authority over memory, entities and worker tasks.

## Proposed decision

Use RAII owners, borrowed spans/references and generation-checked handles. Persistent IDs are independent of ECS indices. EnTT and enkiTS are provisional choices; maintain an explicit phase order and one coordinated worker budget, including physics and streaming. Reserve capacities and bound queues/arenas. Structural ECS changes occur at barriers. Blocking I/O runs outside the simulation worker pool. Expected failures use typed results; isolate exceptions at dependency boundaries.

## Alternatives

Global shared ownership obscures lifetime. A new ECS/scheduler/allocator is not justified. A broad abstract ECS wrapper adds maintenance before a second implementation is required. Flecs/Taskflow remain workload-driven alternatives.

## Consequences

Capacity exhaustion needs visible backpressure/admission/failure behavior. Zero unexpected hot-path allocation is measured after warmup; startup/loading allocation is allowed within budgets. No blanket exception/RTTI flag is imposed on unknown dependencies.

## Acceptance and reconsideration

M1 value/lifetime tests; M3 representative entity churn, allocation traces and 1/2/4/8-worker comparison; TSan on meaningful concurrent paths. Revisit when a trace identifies a concrete scheduler/storage bottleneck.

## M1 scope accepted by the implementation request

The 2026-11-04 request approves RAII, explicit borrowing, strong values, generation checks and bounded work/storage. M1 implements 16 entity slots, 64 commands and 64 pending jobs, with 0–7 worker threads. `std::variant<T, Error>` is the local C++20 result representation. enkiTS 1.12 is the tested scheduler dependency, not a new scheduler or ECS. EnTT and broader memory architecture remain proposed. Jobs borrow callback contexts until draining or shutdown; only cancellation requests may cross the owner-thread API boundary. Startup allocation/configuration exceptions remain exceptions; callbacks must be finite and `noexcept`. See [M1 evidence](../evidence/M1/README.md); independent acceptance is outstanding.

## Evidence

See the [detailed specification and primary sources](../planning/renderer-core-assets.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
