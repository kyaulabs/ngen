# ADR 0001: Separate engine, game and services

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M0/M1; exact scope follows the audited repository

## Context

NGEN must support standalone games while its flagship eventually needs MMO services. Early service dependencies would make offline use and headless testing unnecessarily expensive.

## Proposed decision

Use independently linked core, simulation, presentation and tool targets. Flagship rules/content depend on engine contracts. Network, persistence and settlement are adapters selected by the executable, not mandatory dependencies of the engine. The offline executable owns an in-process session authority; the dedicated server uses the shared simulation without graphics. Start as libraries in the existing repository structure; a new repository or plugin ABI is not required.

## Alternatives

A single executable with global services is simple initially but makes required offline/server separation difficult. A microservice-first engine adds deployment and failure states before the game needs them. A universal plugin system has no present modding requirement.

## Consequences

Interfaces remain narrow value/command/snapshot contracts. Some services cannot reach directly into ECS state. CMake enforces the dependency boundary rather than documentation alone.

## Acceptance and reconsideration

M1 clean CPU harness build/start without Vulkan, SDL video or database services; M5 offline gameplay with network unavailable; inspect transitive dependencies. Revisit only when an actual second game or service integration exposes a missing boundary.

## Evidence

See the [detailed specification and primary sources](../planning/build-review-infrastructure.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
