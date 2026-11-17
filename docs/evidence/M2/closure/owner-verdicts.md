# Owner M2 verdicts

The owner explicitly answered the closure questions after `b2fa7da`:

- **Shader compiler:** “Accept Slang for M2.” The selected pinned offline Slang
  2026.21 toolchain is accepted for this milestone. This does not change licensing
  or accept unrelated ADR decisions.
- **Visual/input slice:** “Needs fixes.” The owner specified that the bounding
  box misses the rear engines/exhaust while covering the rest of the ship. The
  selected primitive was being boxed rather than the assembled asset. A local
  fix now uses full loaded world bounds, independent of material/part selection;
  native regression and actual capture pass without changing bloom. The owner
  subsequently answered **“Yes, bounds fixed”**, accepting this specific correction,
  not unrelated M2 gates.
- **Resize:** Originally “Keep investigating,” with no waiver. The owner later
  reported substantial laptop thermal/CPU-power changes **before both** latest
  operator runs. Focused and visible/unfocused controls both pass; focus alone
  does not reproduce the old stall in that state. A fresh clean 3,600-frame mixed
  run passes the original budgets, maximum 103.571 ms, under current host tuning.
  Earlier failures remain retained; neither an engine repair nor the exact cause
  of improvement is claimed. See [current-host verification](current-host/README.md).

The owner subsequently accepted the blue-white palette but rejected overexposed
engine housings. After a visible localized-light preview they called the result a
“MAJOR improvment” and explicitly requested a plume, superseding the earlier
color/glow-only scope. They then requested a longer tail and wider core/plume.
Their final **“that looks perfect”** accepts the exact
[larger-radius, longer exhaust presentation](exhaust/README.md), package SHA256
`067407fb5fa2527771212cc7280a345b02ad3dd2a823ae8ba81b3553689ec52b`.
No further palette/bounds/exhaust reapproval is requested. This is not an inferred
overall inspector/input verdict or approval of future M3 flight controls. Original
source/canonical assets remain untouched/private. The owner later explicitly
authorized the brand pack, committed unchanged at `d036c91`.

After the exhaust approval, the owner explicitly answered **“Accept
inspector/input”** for the remaining M2 inspector behavior: selection/list,
material/render controls and reload. This is a personal owner verdict, separate
from agent/GitHub account reviews. No further M2 visual/input, palette, bounds or
exhaust approval is requested. It does not accept future M3 flight controls or
replace physical platform verification. The historical compiled Windows UI/bounds
and exact static-package returns are now [verified](windows-final/README.md).

For recurring CI dependency-download 504s the owner selected **“Wait
(Recommended)”**: keep exact pins and checks, wait for upstream recovery and do
not add a CI download/cache fallback as an incidental infrastructure change.

Native Windows schema-3 GPU observation now passes separately at script `5a2f841`
with the unchanged `b2fa7da` binary. Sampled-budget verification is not a new
personal visual/input verdict. Neither these answers nor agent/GitHub account
reviews accept M2 as a whole.
The owner later expanded M2 with moving exhaust and reference-guided materials.
After the visible iterations they explicitly accepted:

- **“Accept exhaust”**: the seeded irregular white-core/blue-fringe motion and
  narrower, longer pointed droplet rooted at the recessed emitters, package
  `9b645de103ea92677c3cfe97cb8bd88ede8e7109f34c0f926888ba65c7522e63`.
- **“Hull is good”**: charcoal-gray coating and the existing panel/wear/normal/ORM
  finish shown in `27de456e…`; not the then-unaccepted cockpit glazing.
- **“Recess windows”**: authorization to cut only six local bridge openings with
  shallow dark backing, preserving the accepted finish/exhaust and window layout;
  no full cockpit model or new glass renderer.
- **“Accept bridge windows”** after the 5,154-frame bridge-only inspection:
  package `a02d5438ad32d9c8fb917ce847d5a822688ca5b51d69125f6da71cf2ba2116ed`,
  using actual 0.9 m recesses rather than transparency over solid hull.

All current visual gates are accepted. Freeze them; do not request those verdicts
again. [Current source/art verification](motion/README.md) includes local native
controls, clean final-package budgets and exact current-host authoring replay.
New scoped Windows motion execution, exact-head CI/review and normal integration
are still required. M2 is not closed by these visual answers.
M3 implementation has not started.
