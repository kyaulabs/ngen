# ADR 0016: Resolve engine and game licensing deliberately

- Status: Proposed
- Date: 2026-11-02
- Decision owner: Sean Bruen
- Adoption gate: M0/M13; exact scope follows the audited repository

## Context

The public source template carries AGPL-3.0 while NGEN may become open source and its flagship may remain proprietary. The actual NGEN license and contributor history are not available.

## Proposed decision

Before publication, inventory current NGEN licensing, copied template/code/assets, copyright ownership, contributor grants and dependency notices. Prefer a permissive engine license such as Apache-2.0 if all necessary rights permit it and Sean chooses that goal. Keep proprietary game content and rules separately identifiable. Do not change any license merely because a folder split or template README suggests a choice.

## Alternatives

Assuming the template license disappeared is unsupported. Assuming every future work must be AGPL without examining rights/derivation is also unsupported. Dual licensing is only available for rights actually controlled.

## Consequences

M0 licensing evidence remains open. Distribution and public service obligations require qualified review where applicable. This proposal is an intake/decision gate, not a legal conclusion or automatic relicensing instruction.

## Acceptance and reconsideration

Actual LICENSE/notice/contributor inventory, identified rights holders, chosen distribution model and documented approval. Verify source/binary/asset release manifests preserve required notices. Reopen when external contributions or restricted dependencies/assets are introduced.

## Evidence

See the [detailed specification and primary sources](../planning/repository-audit.md). This record contains a proposed NGEN decision, not a claim that its tests passed. Record accepted/rejected status, approver, revision and test evidence only after the stated gate.
