# Force-plot tooltip allocation regression

The latest MSVC Release return is again from clean `5ea9e3c`, archive SHA-256
`363c0dd6ad691573b6adb8ac69b30bd8b6d94d1137ca6066e084e6d2003e154f`.
Its 20 CTests pass, but the clean replay reports 2 warm ImGui allocations and the
memory replay reports 0. Its first CIM query again fails with timeout identifier
`0x40004` after 2,015 ms, with no GPU-memory samples. Neither failure is waived.

A native Debug reproduction holds the pointer over FORCE / MN through public SDL
motion events. The full 2,400-frame replay reproduces exactly 2 ImGui allocations.
Stacks identify `ImGui::PlotEx` → `SetTooltipV` → `ImDrawList::PrimReserve` in
`##Tooltip_00`: vertex/index storage allocates 4,680 and 1,008 bytes as numeric text
changes. This explains a concrete hover-dependent allocation path, not a physics
or compiler allocation. SDL window IDs are discovered, never assumed to be 1.

The application primes that tooltip and reserves its existing bounded vertex/
index capacities in a cold GUI frame that is never submitted or presented. The
normal tooltip, styling and counters remain enabled. No overlay/resource limit,
material ABI, accepted appearance or physical actuation is changed. Temporary
allocation tracing/API changes were removed after locating the real call stacks.

The new opt-in public CLI regression `tests/flight_tooltip.py` uses the existing
GDB installation and a native Debug binary/private asset. It is not a runtime or
build dependency. It selects one real case, with zero skips when configured:

```sh
NGEN_FLIGHT_DEMO="$PWD/build/demo-debug/flight_demo" \
NGEN_FLIGHT_PACKAGE="$PWD/build/cooked-candidate/frigate-rcs-directional-v1/scene.ngas" \
SDL_AUDIO_DRIVER=dummy PYTHONDONTWRITEBYTECODE=1 python tests/flight_tooltip.py
```

The budget assertion first fails with 2 allocations, then passes with zero warm
C++/ImGui allocations and all 2,400 GPU rows. Stack/capacity checks verify the
primed tooltip retains 8,192 vertices and 16,384 indices before measurement.
An initial untraced post-fix debugger attempt still reported 2 allocations;
subsequent controlled stack checks and the final permanent regression pass.
Native Windows confirmation remains required, rather than inferring exhaustive
safety from local checks.

Four complete demo graphs pass 88 CTests. Three actual-command Clang analyses of
the sole changed translation unit pass with no stripped flags or changed public
headers. Four physical/control/audio/GUI walkthroughs pass; the ASan walkthrough
passes in an isolated rerun with unchanged checks and strict ASan/UBSan/leak
settings. Its initial concurrent-window attempt reached the angular quiet band
and failed the checker's strict-decrease assertion; that raw attempt is retained.
Do not run focus-sensitive native walkthroughs alongside another GUI client.

A separate full-HD Release replay has zero warm allocations, empty stderr and
all GPU rows; all 27 physical/actuation columns match the physical checkpoint.
This is correctness, not final timing/VRAM/listening acceptance. Same-session
review is not independent review. The WMI timeout, owner-requested F12 overlay,
required later UI refinement and final M3 gates remain open.
