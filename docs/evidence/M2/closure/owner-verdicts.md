# Owner M2 verdicts

The owner explicitly answered the closure questions after `b2fa7da`:

- **Shader compiler:** “Accept Slang for M2.” The selected pinned offline Slang
  2026.21 toolchain is accepted for this milestone. This does not change licensing
  or accept unrelated ADR decisions.
- **Visual/input slice:** “Needs fixes.” The owner specified that the bounding
  box misses the rear engines/exhaust while covering the rest of the ship. The
  selected primitive was being boxed rather than the assembled asset. A local
  fix now uses full loaded world bounds, independent of material/part selection;
  native regression and actual capture pass without changing bloom. Personal
  acceptance of the corrected result remains open.
- **Resize:** “Keep investigating.” The retained resize failure remains blocking.
  There is no waiver, focused-only scope approval or claim of repair.

Native Windows GPU evidence and closure-control corrections remain separate.
Neither these answers nor agent/GitHub account reviews accept M2 as a whole.
M3 implementation has not started.
