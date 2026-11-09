# GPU observation coverage correction

The independent follow-up at `b2fa7da` found a real observer defect: one valid
sample could leave a passing status even when later own-process instances vanished
or only foreign instances remained. Those attempts were not recorded. This is a
measurement defect, not demonstrated renderer memory safety or NVIDIA budget proof.

The corrected schema 3 retains every query attempt, including empty, foreign-only,
zero, error and process-exit results, with parent-clock start/end/duration. Positive
samples remain separate from attempts. Native CSV frame clocks define the measured
interval; explicit startup/exit gaps remain visible outside it. Every missing
active-window observation fails coverage, and successful queries must also satisfy
a maximum one-second observation gap, including measured-window boundaries.
Incomplete coverage has status `incomplete` and a null budget verdict. Native frame
reports require regular/nonlinked bounded CSV input, numeric increasing clocks and
actual parent-clock correlation. CSV expansion is bounded before materialization.

A child exiting during a delayed CIM call no longer hides an observer deadline
overrun. The real child exit code is retained, while `timed_out` is true and the
wrapper fails. CIM remains an arbitrary system boundary: its timeout is not a
hard OS interrupt. Native provider exceptions now retain a bounded message and
HRESULT so an unavailable class/query is diagnosable without printing credentials
or dumping a session. Existing exclusive artifacts and owned-process cleanup stay
in place; no persistent environment, policy, driver or privilege changes occur.

Public-process Red/Green controls reproduce valid-then-empty/foreign-only loss,
active-window gaps, native-window wiring and a late-query deadline overrun. Real
children publish frame clocks and phase markers; only CIM is mocked. Additional
controls cover slow successful queries, invalid clocks, retained diagnostics,
foreign/malformed/zero counters, both budgets and existing process/path failure
controls. The duplicate-instance rejection guard has source-review support, not
a dedicated public-process regression. The final complete PowerShell suite passes on Linux. A separate
extreme-clock hypothesis did not reproduce a metadata-loss defect and is not
claimed as a Red or repaired bug.

These are system-boundary tests, not Windows execution of this corrected observer.
The returned `b2fa7da` Windows workload succeeds but its CIM observation is
unavailable with zero samples. A metadata-only native provider check is pending;
whole-NVIDIA memory acceptance remains open. Positive observed byte maxima are
sampled/provider-refreshed evidence, not exact continuous peaks or global usage.
The old memory handoff remains on hold until a corrected native request is ready.
