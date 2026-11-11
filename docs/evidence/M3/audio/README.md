# Bounded procedural flight audio

M3 now uses original48kHz stereo synthesis for reactor/thrust/braking, UI/reset
and estimated contact-impulse cues. The eight-voice value mixer has no heap,
device, callback or gameplay dependency. Finite normalized admission is atomic;
mute/reset drop stale cues, preserve the mute preference and bound PCM to±0.8.
Sample-count output is independent of caller block cadence.

One owner-thread SDL output feeds copied PCM in at most eight512-frame blocks,
with an input queue capped at4096 frames. SDL owns conversion/playback threads;
they never read mixer/game state. This is not a bound on driver/hardware buffers
or a hard latency guarantee. Native mute pauses and clears the actual stream.
Reported playback loss retires it; added devices or one-second retry reopen it
without replaying old cues. Missing backend/open/queue errors leave explicit
soft-disconnected state, not failed simulation. Close/destruction precede
Context's SDL shutdown; off-owner/closed mutation rejects.

The flight loop maps owning actuation snapshots using the before-tick attitude,
adds subtle engine/thrust/opposing-force sound and UI cues, and estimates impact
from velocity change unexplained by the published wrench. That estimate is not
an authoritative new contact stream. M toggles sound; focus loss/minimization
silence it without changing the user's preference. Reset remains deferred until
the unfrozen frame boundary and clears sound without replacing the GPU asset.
Numeric audio state/generation/loss/queue data accompanies flight CSV.

SDL audio is enabled only for the demo graph. Linux adds shared
`libalsa/1.2.16.1#f1f84174d706f61048a3e55eb5c2daac` (LGPL-2.1-or-later);
Windows keeps SDL's native backend without ALSA. This existing SDL platform
capability avoids adding a sound server/framework. Notices and upstream COPYING
are retained under `licenses/libalsa/`. Other dependency/source pins and lock
timestamps are preserved; only the local SDL recipe revision and demo ALSA
entry change. No private media or copied/reference sound is introduced.

A public driver-list Red found that upstream recipe false options still let
CMake detect ambient PulseAudio/PipeWire/sndio SDKs. The local recipe now honors
false and disables undeclared PipeWire detection. Linux audio exposes only its
declared ALSA/disk/dummy drivers; ordinary rendering keeps audio disabled.
Tablet ownership and SDL's existing Clang shared-sanitizer repair are unchanged.

The first ALSA sanitizer link failed because its DSO `-z,defs` conflicts with
Clang's executable-supplied shared ASan imports. The sanitizer profile scopes
`MAKEFLAGS=LDFLAGS_NOUNDEFINED=` to this client only, following the existing SDL
link model. Normal builds remain strict; all sanitizer/fail-fast flags remain
active. Direct test dependency metadata and `LD_BIND_NOW=1` load the actual
instrumented client and verify its runtime imports. The initial missing direct
library trait was a probe link/setup error, not a runtime Red or waived detector.

Five mixer and six real SDL output cases pass. They cover audible bounded PCM,
voice exhaustion/drain, mute/reset, invalid input/no mutation, block cadence,
stream ownership/queue bounds, actual native pause/clear, reported loss/reopen,
missing-driver recovery and off-owner/closed access. Four complete20/20 graphs
pass on Clang debug/release, strict ASan+UBSan+leaks and GCC; affected header
consumers/tests rerun after an explicit multiplication-width lint repair.
Thirty complete native Clang commands cover new audio/demo headers and targets;
nine affected output-header consumer reruns pass with all flags intact.

Four current330-frame native runs combine rendering, F1 help, SDL controls,
real ImGui reset clicks, reported audio loss/addition and actual M mute toggles.
All GPU rows, finite HDR, pose/generation controls and bounded audio queues pass
with empty stderr. Physics ticks continue through the audio outage. These use
the real SDL dummy playback thread, not physical unplug/listening proof. A quiet
Release run also opens the actual ALSA playback endpoint paused/muted, with
empty stderr. It does not establish subjective quality or physical reconnection.

The audio-disabled inspector graphs and four original native GUI/root probes
remain green; shipping/shader-disabled consumers pass. No new render pass,
GPU owner, shader/material ABI or accepted M2 appearance settings change.
C++/ImGui counts exclude C/driver heaps; sanitizer C++ accounting is explicitly
disabled. VMA is not whole-process GPU memory. Validation correctness runs are
not optimized timing or arbitrary device-loss/global-memory proof. GCC AST
flags remain unsupported and are not stripped; historical vendor/recook limits
remain unchanged.

Parent playable `cf72519` exact-head hosted CI42970251740 passes4/4, including
Clang18 demo checks; this audio source needs its own hosted run. Same-session
Core/C++ review is not independent review. Offline packaging, current Windows,
three warmed laptop runs, scoped RTX performance, recording and owner control/
listening feedback remain. **M3 is not complete.**
