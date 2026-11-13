# ClangCl native collector and F12 return

Archive `c7758867531e1726a51c8feede6123567c1f4848ffcf4e68f75a224500ba7e00`
contains one ClangCl run at signed source `7afeefe`, not the MSVC sibling.
Bounded preflight admitted 85 entries: 1,149,995 compressed bytes and
28,173,203 uncompressed bytes. Selection retained 37 build/identity/test/replay/
counter text files (3,057,641 bytes); their redacted secret scan passed.
Session/bootstrap/dependency payloads were not read or extracted. Returned code
was not executed. The only reported working-tree entry was the owner's untracked
`output.csv`, which was neither read nor removed. Private captures remain outside
Git; eight capture/report hashes were independently checked against archive bytes.

ClangCl 19.1.5 compiled and linked the new native reader in Debug and Release,
using C++20 with extensions off and the dynamic CRT. Both configurations pass
22 CTests, including gauge joining and performance sampling. Two validation-enabled
330-frame walkthroughs pass physical controls, focus/camera/reset, geometry and
reported audio lifecycle checks. Each records three F12 toggles, 260 drawn panel
frames, six OS samples, available system CPU/RAM and selected-device dedicated
VRAM, with no collection failure. Whole-device VRAM is approximately 2.95 GB out
of 12,523,143,168 bytes; it is **not** the process-budget reading below.

The separate 2,400-frame instrumented memory replay supplies 76 observations in
77 attempts. One startup-empty result precedes the measured window. Independent
QPC/frame correlation, row accounting, byte sums and gap recomputation confirm:

- Complete 39.9998336 s measured window; zero missing measured queries.
- Maximum observation gap 570.669 ms, below the unchanged 1 s limit.
- Maximum query 196 ms including initialization; measured queries at most 2 ms.
- Process dedicated + shared peak 362,328,064 bytes.
- Separate committed peak 362,393,600 bytes.
- Working-set high-water 433,553,408 bytes.

Those scoped memory observations meet existing budgets without a timeout,
provider fallback or relaxed admission. Both clean and memory replays have zero
warm C++/ImGui allocation calls and all 2,400 GPU timing rows. Their 27 physical
fields match each other and the historical physical ClangCl replay exactly.
This is not explicit native live-panel hovered-tooltip allocation evidence.

The clean full-HD replay does **not** meet the RTX P95 target: median 14.6328 ms,
P95 21.8084 ms, P99 22.2059 ms and maximum 22.6696 ms, retaining every frame.
The target remains 16.7 ms. Historical clean ClangCl P95 was 7.6696 ms. Individual
GPU-pass medians are similar; this does not identify a CPU, presentation, driver
or scheduling cause. The clean run has F12 hidden, no OS telemetry samples and
no process-memory observer. Do not attribute the miss to the new collector or
change rendering/limits without causal evidence.

App-local startup and geometry verification pass with SDK variables removed and
PATH restricted to the application/Windows search scope. This is not newly
observed network disconnection or complete final delivery-manifest acceptance.

MSVC evidence is still missing. The RTX timing miss, final three-run/post-UI
checks, demonstration recording, explicit hovered-tooltip scope, independent
review and owner final acceptance remain open. No unchanged full rebuild is
requested: inspect the already-produced MSVC sibling next.
