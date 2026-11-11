# Manual takeover cancels stale autopilot

The owner demonstrated repeated TAB returns from direct flight to tactical view
suddenly moving the ship in unintended directions. Review of the private video
finds six mode changes; code tracing shows the previous orbit/destination remained
stored while direct input controlled the ship, then resumed automatically.

A public runtime Red reproduces this for both orbit and destination commands.
Entering direct control now cancels that prior navigation in the staged candidate.
Returning to tactical therefore coasts until a fresh G/O command. Identity,
velocity and ordinary physical integration are preserved; there is no pose reset,
velocity assignment or extra brake. Same-mode confirmation does not cancel an
active goal. Explicit startup/reset orbit and fresh navigation commands still work.
The tactical status line now states the coasting/fresh-navigation policy.

The controller has19 cases. Four21-test demo graphs and four8-test headless graphs
pass, as do fifteen complete native Clang implementation/test invocations. Four
330-frame native SDL/GUI/audio runs pass with empty stderr, finite HDR and all GPU
rows. `check-trace.py` fails on the actual previous native trace, then passes all
four current traces: TAB return publishes coast/zero force/unchanged velocity;
both explicit reset paths still issue a fresh default orbit. This is a same-session
review, not independent human acceptance or hardware timing.

The original owner video, application-only audio and derived combined clip remain
private. The recording belongs to `489fb53`, before this repair; it is useful failure
evidence, not post-fix polished-demo footage. Original SHA256 values and a bounded
metadata record are retained without media bytes. The combined derivative copies
video and aligns native audio using six TAB/880Hz confirmation pairs. The ALSA
file-PCM header tagged native float payload as integer PCM; all float samples are
finite and bounded, and a corrected-format derivative preserves the originals.
Late alignment residual is about0.14s; two recovered ALSA underruns were logged.
Neither perfect synchronization nor glitch-free hardware latency is claimed.

Owner confirmation of the handoff, current-source benchmarks, Windows/RTX,
reconnection and reviewed integration remain open. Human recording clients must
have no frame limit or automatic termination: the owner stops/saves the recorder
and closes the demo themselves.
