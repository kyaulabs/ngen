# Native PDH CSV comparison and collector repair

The third diagnostic archive is
`8524cb6cbda5263f3a1cd41c54948c1c355388e74b3a30b9ea7632758e78705a`:
2,220 compressed bytes, four entries and 6,107 uncompressed bytes. Bounded
preflight selected only the diagnostic JSON, revision and working-tree record
(4,178 bytes). Their redacted secret scan passed. The session was neither read
nor extracted; no returned code was executed. Script revision is `34e6adf`.
The recorded untracked owner `output.csv` was neither read nor removed.

Both explicit CSV captures contain one complete owned-process instance and the
three expected memory gauges. Header identities, integer values, exit codes,
deadlines and process-continuity guards were checked independently. Before/after
PDH helpers completed in 1,244 / 1,209 ms; both 375-byte CSV files agree with the
successful raw-CIM reading:

- Dedicated: 269,930,496 bytes.
- Shared: 53,088,256 bytes.
- Dedicated + shared: 323,018,752 bytes.
- Separate committed: 323,084,288 bytes.

The intervening formatted/raw/formatted CIM attempts failed in
2,048 / 2,014 / 2,021 ms with `HRESULT 0x40004`; the final raw attempt succeeded
in 1,223 ms. This establishes available PDH counter data while the CIM transport
repeatedly misses its existing limit. It does not identify the underlying
Windows defect, establish executable provenance or supply sampled-window budget
acceptance. The two snapshots are not a continuous peak.

The workload observer now uses a measurement-only `gpu_memory_observer` helper,
not `typeperf` or a fallback after CIM failure. It holds one native PDH query,
reads the three English process-memory gauges without scaling, bounds storage
and joins all columns by case-insensitive exact owned instance. Partial,
duplicate, malformed, overflowing and foreign rows are errors. Results own their
names. PowerShell requests one read per existing sample point, bounds responses,
retains native error status, terminates only a stalled owned helper and closes
it on every exit. The 2 s first request includes helper initialization.

Existing 500 ms target cadence, 1 s measured-window maximum gap, failed-query and
all-zero rejection, QPC/frame correlation, 1 GiB GPU and 6 GiB process limits
remain in force. Dedicated/shared/committed accounting is unchanged. Memory
records use schema 4 and explicitly name the PDH transport. The read-only
comparison mode, owner clients, GUI, flight, assets and audio are unchanged.

Verification covers public gauge joining (six cases), four complete Linux demo
graphs (96 CTests), strict ASan/UBSan/leaks, six actual-command Clang analyses,
formatting, four PowerShell suites with Actions-style exit propagation and a
strict MinGW-w64 GCC 16.2 SDK compile/link. Real child fixtures cover query reuse,
shutdown, native failure status, malformed/oversized/EOF output and the 2 s
blocked-reader deadline. Initial behavioral Reds and the enum-size lint finding
were repaired without disabling checks. Same-session review is not independent
review.

At this repair checkpoint the native helper had **not yet executed on Windows**.
The subsequent [ClangCl return](../pdh-collector/clangcl.md) passes sampled memory
coverage, native F12 readings and scoped app-local startup, but misses the RTX
P95 target. MSVC and explicit hovered-tooltip evidence remain open.
The required final M3 UI refinement and integration are not completed by this
repair. See `docs/planning/windows-gpu-memory-collector.md`.

API contracts: [language-neutral counters](https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhaddenglishcounterw)
and [wildcard counter arrays](https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhgetformattedcounterarrayw).
Wildcard paths remain native array queries; no one-time expansion drops later
instances.
