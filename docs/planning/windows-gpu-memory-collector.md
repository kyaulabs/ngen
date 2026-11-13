# Windows process GPU-memory collector repair

Native direct-PDH snapshots now contain the same own-process dedicated/shared/
committed values as raw CIM, before and after three CIM timeouts. The precise
Windows CIM defect remains unknown; that CIM transport did not reliably meet
the existing limits. Do not repair services, registrations or privileges.
An untracked owner `output.csv` in the latest return is not read or removed.

The workload observer now uses that PDH counter family through a native helper;
MSVC and ClangCl native execution now have complete sampled coverage. The
transport change preserves
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

Implementation used public-boundary TDD; final timing acceptance remains open:

1. Complete: owning, bounded three-gauge joins and invalid/duplicate/partial input
   rejection.
2. Complete locally: native English PDH query and bounded request/response
   executable, strict real Win64 SDK compile/link, no fabricated declarations.
3. Complete locally: owned process transport, timeout/EOF/output/error cleanup and
   existing observer policy regressions, including historical failure checks.
4. MSVC/ClangCl Debug/Release compilation, 88 CTests, sampled-window memory and
   native F12 system/device readings pass at `7afeefe`. Explicit live-panel
   hovered-tooltip evidence remains pending. Both clean replays miss the unchanged
   16.7 ms RTX P95 target: MSVC 21.8387 ms, ClangCl 21.8084 ms.

Evidence and check scope:
`docs/evidence/M3/windows/provider-comparison/pdh-csv-return.md` and
`docs/evidence/M3/windows/pdh-collector/{clangcl,msvc}.md`.
The two current clean CSVs complete near 60 Hz versus approximately 144 Hz in
the historical ClangCl run. These are frame-completion rates, not queried monitor
refresh rates. The owner subsequently reported an Artemis Virtual Desktop
session, a plausible but unproved presentation confound. Preserve the functional/
memory results; defer final RTX timing to a direct desktop session after the UI
pass. Do not request an unchanged full rebuild or modify presentation/limits to
hide the timing miss.

Keep the existing GUI, physics, asset, audio and accepted handling unchanged.
Final M3 UI refinement and reviewed integration follow the remaining technical
checks; this repair alone does not close M3.
