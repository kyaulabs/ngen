# PDH reference capture correction

The next private return is archive SHA-256
`c21954c6997c66edc041afcb1e7da90d44b229331cb49c5740302210b9714c2b`
from clean script revision `e669b92`. Preflight admits 2,038 compressed bytes,
4 entries and 5,118 uncompressed bytes. Only the diagnostic JSON and source
identity are selected: 3,205 bytes. The redacted scan passes. The session remains
unread and no returned code is executed.

The same selected demo identity remains live throughout. CIM again reports
formatted/raw/formatted timeouts at 2,024 ms, 1,997 ms and 2,006 ms, then raw data
in 759 ms. All three failures retain WMI identifier `0x40004`, native `Failed` and
status 1. A timeout remains a failure even if the caller's elapsed measurement is
slightly under 2 s. The raw snapshot again has 269,930,496 dedicated bytes,
53,088,256 shared bytes and a separate 323,084,288 committed bytes. This is one
snapshot, not complete coverage or a peak.

The direct-PDH helpers exit 0 in 1,232 ms and 1,706 ms, but their captured stdout
contains only progress/status text. Neither contains a counter header or values.
Therefore this return supplies **no validated direct-PDH reading**. It cannot show
that PDH is healthy or that CIM alone causes the delay. The existing diagnostic
labels tool completion separately from budget acceptance; neither limit nor
formal observer is changed to excuse the missing data.

The capture is corrected by passing `typeperf` an explicit `-o` path inside a
fresh owned temporary directory. After helper completion, only a regular,
non-reparse, nonempty file of at most 128 KiB is read; decoded content is capped
at 65,536 characters. BOM-aware reading preserves native UTF-16 CSV. Stdout/stderr
retain their separate bounds and meanings. A successful exit with no captured
file is now an explicit capture failure, not a completed reference.

The CSV and directory are removed after copying admitted content into the private
diagnostic JSON. No existing file is overwritten. PID/start-time loss discards
CSV as well as stdout/stderr. The 2 s helper budget, four CIM calls and formal
500 ms sampling/1 s coverage/resource gates remain unchanged. Only the owned
helper can be stopped; the demo stays under owner control.

A real-child system-boundary fixture first reproduces the missing-data failure:
exit 0 and status text without requested CSV capture. It then passes with a
separate UTF-16 file. Tests cover status-only rejection, file bounds, cleanup,
existing deadline/stream failures, PDH/CIM ordering and identity-loss discard.
Both existing PowerShell orchestration suites also pass. These local fixtures do
not establish native Windows CSV contents. Same-session review is not independent
review. The next native return must contain actual CSV before drawing conclusions
about PDH or replacing any formal observation path.
