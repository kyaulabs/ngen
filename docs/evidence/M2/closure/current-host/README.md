# Current laptop verification

The owner reports substantial thermal/CPU-power policy changes **before both**
latest operator-assisted runs. The agent did not change, undo or read private
configuration for that tuning. The earlier 200–257 ms resize failures remain
retained; neither a renderer repair nor an exact thermal cause is inferred.

The latest paired diagnostics use the same binary, quality and visible desktop.
Actual SDL state confirms focus throughout the focused run and no keyboard focus
throughout the visible/unfocused run. Mouse-focus changes are not keyboard-focus
changes. All 600 measured rows in each run are retained:

| State | P50 / P95 / max (ms) |
| --- | --- |
| Focused | 16.725 / 18.350 / 105.221 |
| Visible, unfocused | 16.534 / 18.234 / 123.807 |

Focus alone therefore does not reproduce the earlier failure in this host state.
Previous automatic requests and unattended operator attempts did not establish an
unfocused control; they are not used to claim causation. A software-peer diagnostic
aborted before measurement, so it also establishes no production defect. The owner
reported a screen-off/away interruption; SDL focus does not prove physical display
power or visibility. No desktop settings were changed to force a result.

A fresh, predeclared **3600-frame clean mixed run** then passes the original budgets:

- P50 **16.623601 ms**, P95 **18.517679 ms**, P99 **19.230378 ms**,
  maximum **103.570557 ms**; no measured frame is dropped or exempted.
- Two actual resizes, four completed reloads and final generation 5.
- 1280x720 restored after the scripted 1024x640 resize; exposure 1, FXAA on,
  bloom .04, 1024 shadow, 246,080 triangles and 100 asteroid instances unchanged.
  The scripted control edit briefly uses exposure .75/FXAA off at frames 1–2 and
  restores exposure 1/FXAA on at frame 3; every frame remains in the statistics.
- Ordinary stderr empty, finite HDR, 60 warmup frames. Validation is off and
  there is no external interposer/observer; built-in GPU timestamps and allocation
  diagnostics remain enabled. Reload-time C++/ImGui allocations remain visible.

This is passing current-host resize/reload evidence under the original timing
bounds, not a waiver, a claim that old runs passed, or proof of which user change
caused the improvement. The precise old/new policy diff and continuous temperature/
power traces are unavailable. Owner-selected host configuration is part of this
result, not an engine-wide performance guarantee.

Read-only post-run metadata: `intel_pstate`, governor `powersave`, EPP
`balance_performance`, allowed CPU range 400 MHz–4.80 GHz, turbo enabled,
`power-profiles-daemon` active with `balanced`, and thermald/tuned/tlp inactive.
Package RAPL was enabled, with observed limits 15 W / 25 W and a 27.983872-second
long-term window. These are post-run settings, not continuous measured draw,
clocks or proof that every policy stayed fixed during execution. The kernel's
asserted instantaneous frequency is not a hardware frequency measurement.

The owner separately accepted the corrected rear-engine bounds after these runs.
That accepts the specific visual correction, not unrelated M2 gates. The actual
private BMP is inspected but not redistributed; capture identity is retained.
The separate corrected Windows observation now passes native sampled WDDM and
working-set budgets using the unchanged earlier binary; it is not this Linux
run's memory proof. Remaining acceptance stays scoped. M3 has not started.
