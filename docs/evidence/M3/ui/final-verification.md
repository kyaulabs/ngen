# M3 final UI verification

The owner accepted the frozen v25 Visual Identity 1.4 preview and private clip:
“preview and private clip look perfect, continue”. This closes the revised visual
verdict. Earlier explicit handling, audio, disconnected-startup/listening and
native hardware verdicts remain at their recorded sources. Normal reviewed
integration may proceed; this is not a new clean hardware benchmark.

No production source, accepted input behaviour, font/material bytes, capacities,
warmup, allocation detector or test assertion changed during this follow-up.
Temporary phase/backtrace instrumentation and tentative fixture edits were
removed; original file SHA-256 and clean Git diff checks verify restoration.
The delivered v25 source remains `e45ba673040a348959cda48f95e9380a2e9f980e`.

The original, unchanged **20-method GUI suite passes** on an owned, isolated
KWin virtual Wayland display/private D-Bus session, using native Vulkan,
Debug, dummy application audio and explicit validation-layer paths. This covers
context dispatch/dismissal/reopening, drag/hold, overlapping Alt ownership,
viewport limits, rendered material geometry and exact embedded typeface contracts.
Tests that assert zero warm C++/ImGui calls retain those assertions. This is
native UI verification, not physical-monitor, playback, clean timing or independent
review evidence. Earlier active-desktop failures remain retained; isolation does
not prove which original focus, motion, scheduling or timing event caused them.

Host-only execution used installed `kwin_wayland --virtual --width 1600 --height
1000 --socket wayland-ngen-ui --no-lockscreen --no-global-shortcuts --no-kactivities
--exit-with-session <owned-driver>` under `dbus-run-session`. Runtime/config/cache
locations were private under `build/ui-wayland`; the driver ran
`python -m unittest tests.flight_ui -v`. Session lifetime bounded the owned
compositor. Private-session portal autoactivation warnings occurred, not NGEN
test failures. No existing desktop service, profile, configuration or owner
interactive client was changed. KWin/Qt are not NGEN dependencies.

A second actual production recording invocation used the **unmodified frozen
v25 bundle and committed recorder**, without GDB or allocation instrumentation:

```sh
python tools/record_flight.py --bundle build/m3-offline-linux-v25/ngen-flight \
  --output build/m3-identity14-recording-confirmation-v25 --seconds 20
```

It completed 3,600 measured frames / 60 warmup, all GPU rows, finite HDR and
249,942 triangles, with **eight C++ calls / zero ImGui calls**. The own-window
clip retains all 392 frames / 20.099 s and only NGEN's 48 kHz stereo audio through
two stream generations; retained PCM peak 3,656, no microphone/system mix,
unchanged desktop audio graph. Source/manifest identity is the same v25 candidate.
SHA-256: `d2dc3ae96f746204217176c25dc4a626d94a2f35333e9315e5246922987cd3b4`.
The original owner-approved 291-frame clip was not replaced or relabelled.

A separate, explicitly uncommitted diagnostic build traced all C++ allocation
overloads through their shared allocator, without a debugger, while recording
its own window. All eight measured calls resolve to `World::create` /
`Flight::create` through the two deliberately scripted `Flight::reset` operations.
Loop-end/retirement counts were 37,103, baseline 37,095; post-loop HDR readback
then reached 37,866. These observations belong to that instrumented build, not
the frozen executable or a clean benchmark. They distinguish explicit reset and
post-loop readback paths; they do **not** explain the initial frozen media run's
3,854-call count. That original remains untraced and is not accepted as a clean
allocation pass. No font, driver-cache or desktop causal attribution is made.

The walkthrough deliberately replaces physics state and is not the zero-allocation
steady-state budget fixture. Keep both media results and their scopes. The five
current 2,400-frame zero-warm-call hover/input fixtures, unchanged full GUI suite,
108 CTests, strict sanitizer consumers and 15 actual-command analyses supply the
exercised verification; earlier Windows/laptop clean timing/memory evidence is
not rerun or relabelled. Exact `51e07933` push/PR CI both passed all four jobs.

Private logs: `build/m3-flight/private-display-ui-original-all.txt`,
`runtime-native-allocation-trace/`, `runtime-native-stack-symbols.txt`,
`runtime-unmodified-recording-verdict.txt` and `runtime-restored-release-build.txt`.
The restored normal Release target was rebuilt after diagnostic removal.
