# Native GPU provider comparison

The owner returned archive SHA-256
`96374f4941172793f6807a140dcffc76236839f4ed524fa27a74395b65633ce1`
from clean diagnostic script revision `a206b31`. Bounded metadata preflight passes:
1,842 compressed bytes, 4 entries and 4,221 uncompressed bytes. Only the diagnostic
JSON, revision and empty working-tree record are selected: 2,325 bytes. Their
redacted scan passes. The session is neither extracted nor read; no returned code
is executed.

The selected `flight_demo` identity remains unchanged before/after every query.
All four calls retain the 2 s operation timeout:

| Query | Result | Elapsed |
|---|---|---:|
| Formatted GPU process memory | WMI timeout | 2,031 ms |
| Raw GPU process memory | WMI timeout | 2,012 ms |
| Formatted GPU process memory | WMI timeout | 2,011 ms |
| Raw GPU process memory | One valid own instance | 823 ms |

Each timeout has identifier `HRESULT 0x40004`, `CimException`, HResult
`-2146233088`, native `Failed` and status 1. The final raw reading contains
269,930,496 dedicated bytes and 53,088,256 shared bytes: 323,018,752 combined.
Committed memory is a separate 323,084,288 bytes. All fields are finite admitted
integers; the instance matches the selected PID. PID/LUID and session details are
not needed in this public record.

This establishes that the raw provider can return process-memory data while the
same client remains live. It rules out a purely first-query-only explanation and
shows that timeouts are not confined to formatted queries. It does not establish
the underlying cause, exclude an additional formatted-path problem, prove stable
coverage or validate a budget. The diagnostic script revision does not identify
the attached executable's source/compiler, and this does not verify the new
Windows F12 panel or tooltip repair.

The next comparison brackets the unchanged four CIM calls with direct PDH reads
through Windows' built-in `typeperf.exe`. Each helper gets the same 2 s budget,
including its startup, and bounded stdout/stderr. Only the helper can be stopped;
the owner-controlled demo is never terminated. Instance paths target the same
PID. Start-time guards discard output and stop collection if that target changes.
No tools are installed and no services, counter registrations or privileges change.

The PDH reference bypasses CIM but uses the existing OS counters. Its ordering is
explicit because a preceding read may warm a shared backend. Completion means the
native tool returned within its bound, not that CSV data or a memory budget has
been accepted. Counter-name/localization errors are retained, never treated as
zero usage. No timeout, 500 ms sampling target, 1 s coverage gap or resource budget
in the formal workload observer changes.

Public PowerShell Red/Green verifies native-child argument/output preservation and
PDH/CIM/PDH ordering. Real owned-child fixtures additionally exercise timeout
cleanup, output limits, nonzero/missing executables and target-identity loss.
Both existing orchestration suites and the new boundary suite pass locally. Those
fixtures are not native Windows PDH evidence. Same-session review is not independent
review. Native PDH results and final Windows/M3 acceptance remain open.
