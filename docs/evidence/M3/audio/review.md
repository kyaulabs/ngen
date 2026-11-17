# Same-session audio review

Reviewed mixer/output ownership, SDL contracts, dependency options/locks,
application routing, tests and disabled consumers with Core/C++ guidance. This
is assistant-operated review, not independent human review.

- Fixed a demonstrated recipe defect: false backend options did not disable
  ambient CMake SDK detection. The public driver-list Red now passes; ALSA is
  the only declared native Linux backend and all other pins stay unchanged.
- Verified finite atomic levels/cues/span admission, deterministic sample-count
  synthesis, eight-voice exhaustion, bounded magnitude and stale-cue flushing.
  No private audio/reference samples, media decoder or new framework is used.
- Output is nonmovable and owner-thread controlled. SDL copies PCM, owns its
  conversion/playback threads and never calls into game/mixer state. Queue
  filling is at most eight blocks; actual native pause/clear and queue size are
  observed through SDK queries, not fabricated counters.
- Missing driver/device/queue errors are explicit soft-disconnected state.
  Reported removal really destroys the owned logical stream; addition/retry
  opens a new generation. This is not physical unplug proof. Mic/recording
  events are ignored. Reconnect and reset cannot resurrect old cues.
- Main-thread reset remains deferred before frozen frame preparation; it clears
  audio without discarding user mute or replacing GPU resources. Mode/navigation
  cues acknowledge UI intent, not new simulation authority. Impact is explicitly
  an estimate from interval wrench/velocity residual, not a contact event API.
- Fixed SDK verification setup rather than editing generated CMake data:
  direct ALSA calls need a direct test requirement, not a shared SDL library's
  non-transitive library trait. Clang shared-ASan imports follow the existing
  executable-supplied runtime model; normal strict linking remains active and
  eager binding exercises the actual instrumented client.
- A constant PCM-capacity multiplication widened after unsigned arithmetic.
  Make its size_t operand explicit; all nine affected native Clang consumers
  pass without warning suppression or ABI/capacity change.

Four20/20 graphs and affected header-consumer tests pass, as do four330-frame
combined audio/renderer walkthroughs, audio-disabled legacy graphs and four
original GUI/root native probes. Strict ASan/UBSan/leak checks cover exercised
paths, not all interleavings, allocation failures or platform drivers. The muted
actual ALSA open establishes endpoint admission, not subjective quality.

No remaining demonstrated P1/P2 was found in the affected audio paths. Native
SDK open/close may block; the input-queue/retry bound is not a hard system-call,
hardware-buffer or end-to-end latency guarantee. Current Windows, offline
packaging, real listening/reconnection, hardware timing, recording and owner
feedback remain. Existing GCC AST/vendor/recook and global-memory/scanout limits
are not waived; accepted M2 art is unchanged.
