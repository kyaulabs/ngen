# Windows process GPU-memory collector repair

Native direct-PDH snapshots now contain the same own-process dedicated/shared/
committed values as raw CIM, before and after three CIM timeouts. The precise
Windows CIM defect remains unknown; that CIM transport did not reliably meet
the existing limits. Do not repair services, registrations or privileges.
An untracked owner `output.csv` in the latest return is not read or removed.

The workload observer now uses that PDH counter family through a native helper;
ClangCl native execution now has complete sampled coverage, while MSVC evidence
remains pending. The transport change preserves
500 ms target sampling, the 1 s maximum measured-window gap, a 2 s read deadline,
every query attempt, QPC/native-frame correlation, all-zero/missing/malformed/
duplicate rejection, 1 GiB GPU and 6 GiB process limits. No fallback hides failed
measured queries. Dedicated + shared and committed remain separate.

`typeperf` is a diagnostic reference, not a subsecond collector. A small native
measurement-only helper uses the existing Windows PDH SDK/library and English
`GPU Process Memory` counters. No external dependency, Vulkan handle, game pass
or runtime owner is added. The helper owns one query and bounded storage for
64 instance names and three byte gauges. Match all three columns by exact owned
instance, reject partial/invalid sets and copy the published result.

The PowerShell observer owns that helper as a separate child so an unresponsive
PDH call can be stopped at the unchanged 2 s read deadline. Only automated owned
processes are managed; interactive owner clients remain untouched. Prepare helper
startup explicitly, issue one read per existing sample point, bound response
size and close/join the helper on every exit. Native SDK/process failures remain
unavailable observations, not zero usage. The existing testable OS boundary stays
separate from aggregation, coverage and budget policy.

Implementation used public-boundary TDD; native acceptance is still required:

1. Complete: owning, bounded three-gauge joins and invalid/duplicate/partial input
   rejection.
2. Complete locally: native English PDH query and bounded request/response
   executable, strict real Win64 SDK compile/link, no fabricated declarations.
3. Complete locally: owned process transport, timeout/EOF/output/error cleanup and
   existing observer policy regressions, including historical failure checks.
4. ClangCl Debug/Release compilation, 44 CTests, sampled-window memory and native
   F12 system/device readings pass at `7afeefe`. MSVC and explicit live-panel
   hovered-tooltip evidence remain pending. The clean ClangCl replay misses the
   unchanged 16.7 ms RTX P95 target at 21.8084 ms; diagnose before final acceptance.

Evidence and check scope:
`docs/evidence/M3/windows/provider-comparison/pdh-csv-return.md` and
`docs/evidence/M3/windows/pdh-collector/clangcl.md`.
Inspect the already-produced MSVC sibling archive next; do not request another
unchanged full rebuild. The ClangCl return includes the current native helper
beside `flight_demo.exe`. Old attached-process snapshots are not equivalent.

Keep the existing GUI, physics, asset, audio and accepted handling unchanged.
Final M3 UI refinement and reviewed integration follow the remaining technical
checks; this repair alone does not close M3.
