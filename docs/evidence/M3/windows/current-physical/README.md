# Current physical-flight Windows return

Both private archives report clean source `5ea9e3c44fe5fa4660de38624b302ab637a1d1a6`
and the directional asset SHA-256
`99c996f33d3cc98dbd801e3aa302ee2a5894d0fa6bcfdd18f59b73e72e74d834`.
MSVC 194 and ClangCl 19 pass all 20 CTests in Debug and Release: 80 total.
The native 16-bank/32-socket geometry checks and all four physical SDL/control/
focus/reset/reported-audio-loss walkthroughs pass. Both frontends compile and run
the changed owning ship/snapshot layout without disabling warnings or CRT checks.
The owner's v13 physical handling/nozzle approval is recorded separately.

Both clean 2,400-frame RTX 5070 replays at 1920 × 1080 have all GPU rows,
finite/lit HDR, focused playing audio and zero warm C++/ImGui allocations.
Their separate memory replays also have zero warm allocations; the same 27
physical/actuation columns match each compiler's clean run exactly. The historical
2-allocation finding is not reproduced by these current-source runs; no cause or
unverified repair is inferred. Validation walkthrough allocation counts are not
steady-state performance evidence.

| Compiler | Median | P95 | P99 | Maximum |
| --- | ---: | ---: | ---: | ---: |
| MSVC | 7.15885 ms | 7.7625 ms | 8.7228 ms | 9.2985 ms |
| ClangCl | 7.2315 ms | 7.6696 ms | 8.4471 ms | 9.2725 ms |

These are one clean run per frontend, not final three-run/post-UI variance.
VMA allocation/reservation is 232,547,216/235,394,256 bytes in each replay; this
is not process-wide or global GPU use.

ClangCl has 33 successful WDDM samples across 34 retained query attempts. One
empty query is before the native measured window, not an exempted measured query.
Coverage has no missing measured queries, a maximum gap of 601.636 ms and a maximum
query of 277 ms including startup. Sampled dedicated + shared GPU memory peaks at
379,494,400 bytes; committed memory peaks separately at 379,559,936 bytes.
Working-set high-water observation is 433,086,464 bytes. No measures are added to
each other or represented as continuous/global bounds. All six Release artifact
hashes and the Debug hashes verify; BMPs are streamed only for hashes/headers.
The app-local headless/three-frame startup passes with SDK paths absent. Actual
network-disconnected use of this Windows bundle is not established by the script.

MSVC's first memory query fails after 2,048 ms. The new identifier is
`HRESULT 0x40004`, matching .NET's `ManagementStatus.Timedout` (`262148`). The CIM
native error is `Failed`, status 1. This identifies a timeout, not its underlying
cause. The failed run has no GPU-memory samples and is not accepted; its
433,508,352-byte working set and VMA cannot substitute. The run stops before
Release artifact-manifest/package/app-local startup. Preserve this failure and
repeat the unchanged MSVC Release check; do not relax timing, coverage or budgets.

Both ZIPs passed bounded metadata/type/path/count/size/compression/duplicate/
encryption preflight. Only explicit revision, test, replay, memory, frame and
validation/configuration files were extracted and redacted-secret-scanned.
Session, bootstrap, dependency-build and executable payloads remain unread;
returned code is never executed. No private media/assets are committed.
`checks.json` records archive identities and scoped results. Same-session review
is not independent review. MSVC memory/startup, required UI refinement, final
budgets and reviewed integration still keep M3 open.

A subsequent unchanged MSVC Release return again times out and reproduces 2 warm
ImGui allocations in its clean replay. The [force-tooltip investigation](../../tooltip/README.md)
now reproduces a concrete hover-dependent allocation path locally and primes its
bounded buffers. Native Windows confirmation of that fix remains pending; the
new result supersedes treating the earlier zero-allocation runs as resolution.
