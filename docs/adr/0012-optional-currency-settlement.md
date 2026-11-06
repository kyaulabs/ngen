# ADR 0012: Keep external currency settlement optional

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M7/M12; exact scope follows the audited repository

## Context

The desired externally tradable currency introduces accounting, custody, fraud and legal obligations beyond ordinary game progression.

## Proposed decision

Build closed-loop GameCredit first without a peg or promised conversion. If public tokens later pass feasibility, define a distinct asset and a funded, reviewed conversion/issuance policy. Keep an authoritative journal and an asynchronous settlement service outside the tick. Preserve deposit/withdrawal states, holds, finality/reorg evidence and consistent-position reconciliation. After signing/broadcast, unknown outcome does not release the withdrawal hold. Finalized external transfers are corrected through reviewed compensation, not world rollback.

## Alternatives

Chain-confirmation-dependent gameplay is rejected. Unlimited game-credit conversion into unfunded redeemable claims is rejected. Multiple chains, custom bridges and complex token mechanics add unproven risk.

## Consequences

The game can ship with external transfer disabled. Custody outsourcing is an option, not a declaration of exemption. Legal/security approval depends on actual operator, money flow and jurisdictions.

## Acceptance and reconsideration

M7 economic invariants, then M12 written legal/cost/custody/security go/no-go and fault-injected test settlement. Public transfer requires separately authorized release with reviewed reserves, operations and incident recovery.

## Evidence

See the [detailed specification and primary sources](../planning/blockchain-economy.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
