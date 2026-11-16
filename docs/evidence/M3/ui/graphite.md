# Graphite panels and private Linux recording

The owner reports that real disconnected startup and listening passed, but
withholds M3 acceptance pending this final Graphite UI revision. PR #10 stays
draft, unapproved and unmerged. Earlier accepted handling/audio and hardware
checks remain scoped to their recorded sources; no timing or memory gate was
reopened for a theme change.

The supplied replacement pack has SHA-256
`b563a1a7dd8616a9f2e3ec85940c57e9349144d87e5554e545caeb5f6f05fc33`.
Bounded ZIP preflight admitted 121 entries / 6,366,709 expanded bytes from a
4,183,379-byte archive; redacted scanning passed. No archive content was executed.
Its Graphite 1.2 artwork/tokens replace the previous `brand/` inputs. Background,
panel and header tokens are `#202124`, `#2A2B2F` and `#3A414E`.

Flight utility, performance and selected-target context surfaces have disjoint
header/body fills and a pixel-aligned outer border. Their actual drawn RGBA is:

- Panel: `(42, 43, 47, 166)`.
- Header: `(58, 65, 78, 166)`.
- Border: `(120, 153, 166, 166)`.
- Text alpha: `255`.

Opacity is flight-local, not a global text/style fade. The inspector adopts shared
RGB tokens while retaining its layout, font sizing and opacity. No new renderer
owner/pass, material ABI, geometry, capacity, gameplay or audio-controller change.

The initial rendered Red observed the old `(14, 34, 44, 128)` panel. Review then
found double half-pixel alignment in the border; a draw-geometry regression
observed its inner edge at 1.5 px over a fill inset of 1 px. Removing the caller's
extra alignment passed the geometry and all three panels' RGBA checks. Disjoint
fills avoid compositing a translucent header over a second full panel layer.

Verification includes 108 CTests across Clang Debug/Release, strict ASan/UBSan and
GCC; 19 native UI cases; five 2,400-frame hover/input fixtures; four non-ptrace
leak-enabled sanitizer UI cases; and Debug/ASan validated inspector probes. Six
full actual-command GUI/app Clang analyses and the separate native recorder
analysis pass without disabling checks. GCC builds pass; no GCC AST claim. An
initial compact-viewport test lacked validation-layer paths; it passed with the
required paths, not after a product repair. Private controls/data captures were
opened and visually inspected.

`tools/record_flight.py` builds the optional host-only Qt/KPipeWire/Wayland helper
and records only its own sandboxed NGEN descendant's unique KDE window. Native
microphone/system-audio flags are disabled and checked. Exact-PID Pulse sink-input
selection plus `parec --monitor-stream` excludes other playback clients. No
microphone/default-input/whole-display/KMS/X11 fallback, desktop installation,
service/profile/mixer change or new NGEN runtime dependency. Host-specific
protocol and licensing limits are in the tool's README and notice.

Review found and reproduced missing launcher hash admission, SIGTERM skipping
teardown, asymmetric PCM saturation and checking nonzero audio outside the final
video interval. Repairs require the launcher digest, route termination through
cleanup/nonzero failure, reject both saturation endpoints and validate the exact
aligned PCM sent to the muxer. Six portable policy cases cover selection, PCM and
reported pidfd failure;
the private asset boundary and actual CLI cancellation are opt-in native cases.
Native cancellation returned 130, retained failed `recording.json` and left no
owned NGEN process. A later cancellation run exposed child teardown racing the
bubblewrap supervisor's exit. Cleanup now signals the verified owned child through
a pidfd while its supervisor can reap it, before stopping the supervisor. The
eight-case native policy suite passes this repair, including cancellation both
before window discovery and during recording. SIGTERM/Ctrl-C are deferred to managed
safepoints so they cannot interrupt `Popen` before its handle is retained. pidfd
errors remain nonzero recorded failures rather than skipping metadata/teardown.
A pending cancellation is checked after final desktop validation and again before
writing success metadata; late requests cannot be published as successful clips. Review is separate from native
execution and owner acceptance.

A five-second end-to-end tool pilot against **old v23/source `c27f1ec`**, not the
Graphite candidate, produced 105 own-window frames at 2580 × 1506 and nonzero
48 kHz stereo PCM across two NGEN sink-input generations. Its desktop audio graph
was unchanged. The complete instrumented 3,600-frame walkthrough reported eight
warm C++ calls and zero ImGui calls; the cause is untraced. This is not allocation
or clean-budget evidence and does not replace accepted measurements. The private
pilot hash is `49e6788b44388b54a231eb6c790d6e3430771b37ffd356a6c228e6c2d40c6727`.

The final Graphite source-bound bundle/clip identity follows in the delivery
record. Private media/assets stay outside Git/public uploads. Monotonic audio
alignment is approximate, not sample-accurate AV certification; source review,
nonzero/unclipped PCM and automated recording cannot supply listening comfort or
final owner acceptance.
