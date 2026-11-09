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
source/canonical assets and the unrelated brand pack remain untouched/private.

Native Windows schema-3 GPU observation now passes separately at script `5a2f841`
with the unchanged `b2fa7da` binary. Sampled-budget verification is not a new
personal visual/input verdict. Neither these answers nor agent/GitHub account
reviews accept M2 as a whole.
M3 implementation has not started.
