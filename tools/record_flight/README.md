# Private flight recording

From the repository, run:

```sh
python tools/record_flight.py --bundle build/m3-offline-linux-v24/ngen-flight
```

Use a fresh source-bound Linux bundle. The command builds the small native capture
helper, launches its own NGEN process with networking unshared, records its window
and playback stream, and writes a private `recording.mkv` plus identity/diagnostic
metadata under a fresh `build/flight-recording-<id>` directory. `--seconds` accepts
5–30 (default 20); `--output` must name a new directory under project `build`.
Failures retain their diagnostics and return nonzero. This is bounded automated
verification, not an owner preview or a clean performance measurement. Launch the
bundle's `run.sh` separately for an unlimited owner-closed preview.

The supported development host is KDE Wayland with the inspected version-6
screencast protocol and existing Qt 6, KPipeWire and Wayland development packages.
Clang, CMake/Ninja, `qdbus6`, `bwrap`, `pactl`, `parec`, FFmpeg and ffprobe must
already be installed. Missing capabilities fail explicitly; nothing is installed
and no display/audio service, counter, privilege, mixer or device profile changes.

The native helper accepts only a `flight_demo` descendant of its caller, resolves
that PID's unique window through a temporary owned KWin script, and streams only
that window through PipeWire. It uses object serials rather than reusable node IDs.
Its system-audio and microphone options are explicitly disabled and checked.
There is no monitor, region, KMS or X11 capture fallback. The optional native tool
uses the installed desktop SDKs because this host's FFmpeg has no PipeWire input
and GStreamer's `pipewiresrc` is absent. Those SDKs do not enter NGEN's build graph
or runtime bundle. The host-specific protocol and license limits are recorded in
`licenses/plasma-wayland-protocols/NOTICE.md`.

Audio is selected only from the launched NGEN PID's unique Pulse sink input and
that sink's monitor. `parec --monitor-stream` excludes other playback clients;
no microphone or whole-system mix is used. Native stream-reopen segments and
observed PCM counts are retained. Alignment uses monotonic capture times, not
sample-accurate AV timestamps; gaps remain silence rather than another source.
The original raw capture remains private alongside the aligned/muxed file.

Only owned subprocesses and the uniquely named temporary KWin script are cleaned
up. Existing owner applications and prior recordings are untouched. The clip hash,
nonzero/unclipped stereo PCM and unchanged desktop audio graph are verified, but
recording does not establish listening comfort or overall owner acceptance. Do
not commit or publicly upload the private asset bundle or recording.
