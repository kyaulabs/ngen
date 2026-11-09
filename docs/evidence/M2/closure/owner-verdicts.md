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

Native Windows schema-3 GPU observation now passes separately at script `5a2f841`
with the unchanged `b2fa7da` binary. Sampled-budget verification is not a new
personal visual/input verdict. Neither these answers nor agent/GitHub account
reviews accept M2 as a whole.
M3 implementation has not started.
