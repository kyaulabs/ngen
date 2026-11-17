# Economy and optional blockchain settlement

Status: proposed architecture and legal-review intake, researched 2026-11-02. No economic implementation, wallet, contract, custody provider or legal classification was tested. External transfer is optional and must not block the core game.

## Start with a closed-loop currency

M5 introduces earned `GameCredit`, usable inside the game without withdrawal, a dollar peg or promised future conversion. M7 makes its accounting durable. External tokens belong to a separately approved M12 workstream. Offline games need a local transaction adapter and no online service.

If a later release offers redeemable tokens, create a distinct `AssetId` and publish a fair conversion plan before migration. Define eligible balances, snapshot date, funded allocation, rate, caps, anti-abuse rules, appeals and treatment of existing players. Obtain legal review and allow informed participation. Never retroactively describe earlier free credits as backed tokens. Existing credits remain usable under the disclosed game rules unless a reviewed migration explicitly replaces them.

## Accounting contracts

| Owner | Contract |
|---|---|
| Game economy | Validates rewards, markets, resource costs and issuance policy; requests journal operations. |
| Ledger/persistence | Atomically commits postings, inventory transfers, reservations and outbox events; enforces idempotency. |
| Settlement service | Optional adapter for external deposits/withdrawals; cannot alter simulation state directly. |
| Signing boundary | Custody provider or isolated signer; unavailable to clients, simulation workers and CI. |

Use checked integer subunits and explicit asset identity. Choose supply limits and database representation together; reject overflow and scale mismatch. Each immutable journal transaction records its unique source event, actor, authority epoch, policy version and balanced postings. Postings sum to zero per asset. Player balances cannot become negative; issuance and sinks use explicit control accounts.

An order reserves buyer money and seller inventory. Each fill commits payment, fees, item transfer, order state and outgoing event together. Unique item IDs or conserved stack quantities prevent duplication. Repeated commands return the original result. Outbox delivery may repeat, so consumers must also be idempotent. Scripts receive narrow commands such as reward requests, never arbitrary balance or SQL access.

Reservations have explicit states: `held`, `partially_consumed`, `consumed`, `released`, or `quarantined`. Market cancellation releases only the unfilled portion. Concurrent cancel/fill attempts serialize against the same reservation. A failed external request does not release funds unless its terminal outcome is established.

Audit exports must cross a separate trust boundary. Hashing journal rows beside the database alone does not protect them from an administrator controlling both. Correct errors with compensating entries; preserve the original evidence.

## Economy controls

NPC factories reserve real inputs, complete durable job IDs once and apply bounded offline catch-up. NPC purchases consume recorded budgets. Territory changes and destroyed factories need explicit policies for reserved materials and queued production.

Measure currency creation/destruction by cause, goods prices, active-player balances, wealth concentration, inventories and new-player recovery time. Simulate population growth and collapse, wars, bottlenecks and bot saturation before changing rewards. Ship loss removes goods; it does not automatically destroy currency. Distinguish fees paid to a treasury from currency permanently removed.

Preserve free recovery without creating a farming loop. Proposed starter hulls cannot be transferred, sold or salvaged for value and receive no currency insurance payout. Test deliberate loss, account creation, colluding kills, repeated tutorial rewards and inventory handoffs. Apply reward limits and anomaly review at economic boundaries. Client detection alone cannot establish reward legitimacy.

## Optional token and reserve model

Evaluate one canonical token on one chain before considering cross-chain support. Compare a regulated custody provider with self-custody; outsourcing does not automatically remove NGEN's obligations. An EVM prototype can use OpenZeppelin Contracts 5.x ERC-20 and access-control components, with the exact release pinned and independently reviewed at M12. Custom bridges, rebasing and upgrade proxies need separate justification. [1, 2]

Internal redeemable balances represent custody claims. At a common reconciled position, let `R` be eligible finalized token reserves and `L` all customer claims, including market holds and pending withdrawals. Require **`R >= L`**. Track unfinalized deposits, signed transfers, finalized-but-unposted transfers, fees and treasury holdings separately. Compare compatible chain/database positions; unrelated “latest” balances cannot prove solvency.

Allocate backed treasury units before granting redeemable rewards, or complete a controlled issuance process before making them redeemable. Reserve tokens and their internal claims represent the same value and cannot both count as circulating supply. Token backing is not dollar backing or a liquidity guarantee.

Deposits follow `detected → validated → confirmation_pending → finality_qualified → credited`, with exceptional reversal recorded separately. Validate chain, canonical contract, recipient, decimals, amount, transaction success and block history. Identify events by chain ID, transaction hash and log index while retaining block-hash/reorg evidence. Unconfirmed deposits cannot fund withdrawals.

Withdrawals follow `requested → reserved → risk_approved → authorized → signed → broadcast → included → finality_qualified → settled`. A rejection before signing releases funds. After signing, a timeout means unknown outcome. Reconcile the same operation and nonce/transaction family before retrying or refunding. Fee replacements remain attached to the original withdrawal.

A finalized external transfer cannot be reversed by restoring the database. A deep reorg triggers reconciliation, affected-funds quarantine and the published recovery/liability policy, rather than a galaxy rollback. Use withdrawal limits, hot-wallet limits, restricted signing, independent exceptional approval, recovery drills and a settlement pause. Gameplay continues during a chain outage.

## Washington and federal review gates

These are conditional requirements and questions for qualified counsel. Operator identity, actual money flows, custody rights, marketing, redemption terms and user jurisdictions determine applicability.

| Area | Verified point and required decision |
|---|---|
| Washington scope | RCW 19.230.010(18) excludes game units with no outside market/application. External trading changes that premise. A closed-loop label alone does not establish exemption. [3] |
| Washington custody | Applicable applicants storing virtual currency need a third-party systems audit. WAC 208-690-060 sets $100,000 minimum tangible net worth for virtual-currency storage. Budget applicable licensing, capital, bonding and operations only after classification. [4, 5] |
| Backing/disclosures | Licensed virtual-currency transmitters must hold like-kind consumer backing; disclosures address fees, insurance, irreversible transfers and error handling. [6, 7] |
| FinCEN/BSA | Administrators, exchangers and hosted-wallet operators require activity-specific analysis of MSB/AML obligations. Calling rewards “earned” does not decide classification. [8] |
| OFAC | US sanctions obligations also apply to digital currency; determine screening, blocked-property handling and escalation. [9] |
| Securities/commodities | The March 2026 SEC interpretation distinguishes asset classification from investment-contract transactions. CFTC describes spot fraud/manipulation and derivatives authority. Review distribution, fundraising and profit claims. [10, 11] |
| Tax | IRS treats digital assets as property and describes custodial broker reporting, including gross proceeds from 2025 and some basis reporting from 2026. Determine applicable operator/provider duties and records. [12] |

The SEC's August 18, 2026 Regulation Crypto Assets is a proposal in the retrieved record, not an assumed exemption. The GENIUS Act addresses payment stablecoins under a definition involving fixed monetary redemption and stable-value representations; it does not automatically cover every game token. The OCC's October 21, 2026 final-rule bulletin states a January 18, 2027 effective date. Recheck law at launch. [13–15]

Review other user jurisdictions, consumer protection, privacy, minors and abandoned property before public transfer. Any future paid chance-based rewards need their own assessment. Engine licensing and asset redistribution rights are separate questions from financial regulation.

## Tasks and completion gates

All numerical tests below are proposed acceptance targets, not measured results.

| Stage | Atomic tasks | Completion evidence |
|---|---|---|
| M5 | Define money limits; implement local journal/reservations; connect market and reward commands; test starter recovery. | Duplicate/reordered commands cannot create value; item and currency conservation hold. |
| M7 | Add durable journal/outbox; migrate reservations; implement factory catch-up; exercise restore. | One million commands grouped into seeded sequences; zero unexplained imbalance or duplication. Retain seeds and inject crashes at transaction/publication boundaries. |
| M12 feasibility | Document money flows, custody choices, conversion plan, jurisdictions, capital and staffing; obtain legal/security review. | Written go/no-go and funded operating plan. A no-go leaves the game functional. |
| M12 prototype | Model settlement; integrate one test token; inject reorgs, duplicate notifications, lost replies, RPC disagreement, fee stalls and signer outage. | No duplicate credit/payment; unresolved outcomes stay reserved; clean reconstruction from backups and chain history. |
| Public transfer | Complete independent contract/custody review, applicable registrations, reviewed terms, reserves and incident drills. | Named accountable operator, verified reconciliation/restore, resolved findings and separately authorized staged release. |

## Sources

1. [OpenZeppelin ERC-20](https://docs.openzeppelin.com/contracts/5.x/erc20)
2. [OpenZeppelin access control](https://docs.openzeppelin.com/contracts/5.x/access-control)
3. [RCW 19.230.010](https://app.leg.wa.gov/rcw/default.aspx?cite=19.230.010)
4. [RCW 19.230.040](https://app.leg.wa.gov/rcw/default.aspx?cite=19.230.040)
5. [WAC 208-690-060](https://app.leg.wa.gov/wac/default.aspx?cite=208-690-060)
6. [RCW 19.230.200](https://app.leg.wa.gov/rcw/default.aspx?cite=19.230.200)
7. [RCW 19.230.370](https://app.leg.wa.gov/rcw/default.aspx?cite=19.230.370)
8. [FinCEN FIN-2019-G001, May 9, 2019](https://www.fincen.gov/system/files/2019-05/FinCEN%20Guidance%20CVC%20FINAL%20508.pdf)
9. [OFAC FAQ 560](https://ofac.treasury.gov/faqs/560)
10. [SEC interpretation, March 17, 2026](https://www.sec.gov/files/rules/interp/2026/33-11412.pdf)
11. [CFTC digital asset jurisdiction](https://www.cftc.gov/LearnAndProtect/digitalassetfrauds)
12. [IRS digital assets](https://www.irs.gov/filing/digital-assets)
13. [SEC proposed Regulation Crypto Assets, August 18, 2026](https://www.sec.gov/newsroom/press-releases/2026-76-sec-proposes-new-regulation-crypto-assets)
14. [GENIUS Act, Public Law 119-27, July 18, 2025](https://www.govinfo.gov/content/pkg/PLAW-119publ27/html/PLAW-119publ27.htm)
15. [OCC Bulletin 2026-50, October 21, 2026](https://www.occ.gov/news-issuances/bulletins/2026/bulletin-2026-50.html)
