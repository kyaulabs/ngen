# F12 performance overlay

F12 toggles a detailed panel, hidden on startup. Fresh presses from the flight
window change its preference; repeat/key-up/foreign-window events do not. Camera,
focus and scene resets preserve visibility. The existing ImGui/Context overlay
renders it; no renderer pass, GPU owner, material ABI or resource limit changes.

The panel shows FPS/frame time; completed shadow, PBR, post and UI GPU intervals;
VMA allocated/reserved bytes; process/context-lifetime C++/ImGui allocation call
counts; and whole-system CPU/RAM plus selected-device VRAM information. Allocation
call counts are not heap size or the CLI's separate warm-window counts. Sanitizer
builds label the unavailable C++ allocation probe instead of implying zero calls.

Linux reads aggregate `/proc/stat` and RAM total minus available from
`/proc/meminfo`. Iris Xe is explicitly shared/UMA, not dedicated VRAM. Windows uses
system CPU/RAM APIs and the English PDH `GPU Adapter Memory` dedicated-usage gauge,
bound to the copied Vulkan adapter LUID/node. Unsupported/shared/ambiguous adapter
identities, including multi-node configurations, are not guessed. Windows CPU
sampling is unavailable across multiple processor groups rather than reporting
only one group as the whole system. No external package is added; `pdh` is an OS
SDK/library dependency.

One worker collects roughly once per second, outside the snapshot mutex. It owns
no Vulkan handle and pauses while F12-hidden or the interactive window is
minimized. Snapshots are copied; readings older than 3 s display as unavailable.
Exceptions and thread-creation failures expose collection failure without stopping
flight. Shutdown requests stop under the condition-variable mutex, wakes and joins
before frontend/Context teardown. Native OS calls remain cooperatively joined,
not a hard wall-clock shutdown guarantee.

An initial native test execution timed out after 180 s; no blocked stack was
captured. Review found a real lost-notification window when requesting stop outside
the condition-variable mutex. The synchronized repair, controlled blocked-reader
case, 500 disabled-worker construction/destruction iterations and separate TSan
lane pass. They do not prove which stack caused that original timeout or every
possible interleaving.

Cold, unsubmitted GUI preparation reserves the existing bounded tooltip/panel draw
storage. Four complete demo graphs pass 92 CTests: Clang Debug, Release, strict
ASan/UBSan/leaks and GCC Debug. The sampler suite has 9 cases/560 assertions; updated
tests and a separate sampler TSan execution also pass. Nine Clang analyses cover
the implementation and both header consumers in all three Clang configurations,
with actual compile commands and no stripped flags. TSan covers owned sampler/test
code, not graphics or uninstrumented dependency internals.

Native F12/hidden-startup cases pass all four configurations; the final strict
ASan native rerun also passes. The opt-in Debug hover fixture passes both hidden
and live-panel cases: each has 2,400 full-HD frames, zero warm C++/ImGui allocations
and all GPU rows. GDB results are not timing or sanitizer-under-ptrace evidence.
A separate untraced Release replay also has zero warm allocations, empty stderr,
all GPU rows and identical values in all 27 physical/actuation columns against the
previous physical checkpoint. The native panel capture was visually checked;
private asset/capture bytes remain outside Git.

```sh
NGEN_FLIGHT_DEMO="$PWD/build/demo-debug/flight_demo" \
NGEN_FLIGHT_PACKAGE="$PWD/build/cooked-candidate/frigate-rcs-directional-v1/scene.ngas" \
SDL_AUDIO_DRIVER=dummy PYTHONDONTWRITEBYTECODE=1 python tests/flight_tooltip.py
```

`tests/flight_performance_ui.py` exercises real F12 routing, mode/focus handoff,
keyboard/widget reset and hidden/no-poll startup. Supply the same native paths and
normal validation-layer configuration; use the strict sanitizer environment for
ASan. These are opt-in GPU tests, not headless CTests or listening proof.

The Windows source passes a strict real Win64 GCC cross-compilation, but native
MSVC/ClangCl and PDH readings still need confirmation. This whole-device display is
not MSVC's failed per-process CIM budget observer and does not replace its coverage,
timeout or limits. Same-session review found no further task-related defects; it
is not independent review. Windows confirmation, the required broader UI pass and
final M3 acceptance/integration remain open.
