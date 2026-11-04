# ADR proposals

All records are **Proposed**. Numbering is local to this planning package; reconcile it with the real NGEN ADR index before importing. No record is accepted merely because this file exists. Decision owner: Sean Bruen. Reviewers provide recommendations and evidence.

| Record | Decision | Earliest gate |
|---|---|---|
| [0001](0001-engine-game-service-boundaries.md) | Separate engine, game and services | M0/M1 |
| [0002](0002-vulkan-capability-baseline.md) | Use a Vulkan 1.3 capability baseline | M0/M2 |
| [0003](0003-hierarchical-coordinates.md) | Use hierarchical coordinates and frame epochs | M1/M4 |
| [0004](0004-local-physics-and-flight.md) | Separate astronomical travel from local contacts | M3/M4 |
| [0005](0005-ownership-memory-ecs-jobs.md) | Keep ownership and scheduling explicit | M1/M3 |
| [0006](0006-commands-and-time.md) | Make commands and time explicit | M1/M3/M6 |
| [0007](0007-renderer-organization.md) | Start with fixed PBR passes and measured Forward+ | M2/M3/M11 |
| [0008](0008-cooked-assets-and-shaders.md) | Compile versioned assets and shaders offline | M2/M4 |
| [0009](0009-authoritative-networking.md) | Use authoritative snapshots and local flight prediction | M6 |
| [0010](0010-transactional-persistence.md) | Commit economic effects transactionally | M5/M7 |
| [0011](0011-regional-scaling-and-fencing.md) | Scale regions with explicit clocks and fenced ownership | M9/M10 |
| [0012](0012-optional-currency-settlement.md) | Keep external currency settlement optional | M7/M12 |
| [0013](0013-bounded-gameplay-scripting.md) | Use bounded first-party gameplay scripts | M5 |
| [0014](0014-reproducible-build-review-ci.md) | Preserve repository policy in builds and reviews | M0/M1 |
| [0015](0015-development-art-provenance.md) | Keep generated art in a measured development pipeline | M2/M3/M11 |
| [0016](0016-license-and-proprietary-game.md) | Resolve engine and game licensing deliberately | M0/M13 |
