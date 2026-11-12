# Shared Linux desktop playback

NGEN explicitly selected direct ALSA while PipeWire/WirePlumber managed the same
card. The owner's Speaker sink disappeared and other applications lost playback
until a HiFi → Pro Audio → HiFi profile cycle. The desktop journal reported
hardware playback `Device or resource busy` and subsequent Speaker-sink creation
failure. NGEN must be a desktop-server client, not compete for hardware ownership.

Linux flight now uses only SDL's declared PulseAudio client backend, compatible
with PipeWire's existing PulseAudio service. The forced ALSA hint and ALSA SDK
requirement are removed; ALSA and sndio are disabled, with no hardware fallback.
Caller preferences remain SDL's responsibility. Missing server initialization
leaves Output soft-disconnected; it does not stop simulation. No desktop profile,
service, volume, device configuration or audio gain was changed for this repair.
Windows keeps its existing native backend; other render/inspector graphs remain
audio-disabled. The synthesizer, PCM bounds and physical flight are unchanged.

The SDK driver-list regression first rejected the old ALSA backend, then passed
with the desktop backend. The bundle-manifest regression likewise first found
`libpulse.so.0` missing and is now Green. The runtime ABI list and private launch
instructions now require the platform client and desktop service, not direct
`libasound.so.2`. **Do not use Linux flight bundles through v12 for new playback:**
they retain the unsafe direct route. The accepted private directional asset and
all 27 physical/actuation fields across the 2,400-frame replay are unchanged.

Real desktop playback then found two additional native dependency defects:

- Strict UBSan diagnosed a property destructor called through an incompatible
  function pointer in PulseAudio 17.0. The labelled, pinned client-only
  `callback-safe` recipe accepts the declared `void *` callback argument and casts
  inside the function. Destruction behavior is unchanged.
- Strict LeakSanitizer reported 178 bytes of SDL error TLS on PulseAudio's foreign
  callback thread. A debug duplicate-handle assertion performed an expected-
  absence lookup that set `Device not found`. SDL's labelled third patch retains
  the assertion and public lookup errors, but uses a non-error lookup for that
  assertion. No assertion, detector or leak check is disabled.

Both native Reds are preserved and the actual desktop open/play/close fixture now
passes ASan/UBSan/leak checks with eager binding. Necessary client dependencies,
source checksums and notices are pinned. Unneeded codecs/programs, ambient MPEG,
systemd and asynchronous-DNS integrations are disabled through their actual
native configuration. The Pulse-only sanitizer Meson file allows executable-
supplied Clang instrumentation imports; ordinary linking remains strict.
SDK/header/backend loading uses `LD_BIND_NOW=1`, including real playback tests.
See the [client recipe](../../../../recipes/pulseaudio/README.md) and
[notice](../../../../licenses/pulseaudio/NOTICE.md).

Final local verification:

- Four complete demo graphs: Clang Debug, Release, strict ASan/UBSan/leaks and GCC,
  22 CTests each. The audio-disabled Release inspector adds 12 passing CTests.
  The affected output test was rebuilt/rerun after adding desktop integration.
- The opt-in real-desktop lifecycle case passes all four configurations, with
  424 assertions each. Two Output streams run together; closing one leaves the
  other playing. Default CTest runs intentionally use dummy audio and do not
  select this service-dependent hidden case.
- Six full actual-command Clang analyses cover the changed Output implementation
  and test in Debug, Release and ASan. No public C++ header or ABI changes.
- Two independent native NGEN processes and `paplay` concurrently appear as
  PipeWire playback clients. Eight overlapping samples retain all sink IDs,
  active ports, card profiles and the default sink. NGEN has no `/dev/snd/`
  descriptors. Other-application playback succeeds during and after close.
- Actual rendered/physical flight runs 330 measured frames on the real desktop
  backend while another player works. Forty-one server-stream samples retain the
  Speaker/default/card graph and find no hardware sound descriptors. Playback
  still works after flight exits; audio reports playing, no losses and a bounded
  4,096-frame queue. Warm C++/ImGui allocations are zero.
- A missing-server sanitizer probe returns explicit soft-disconnection, not
  hardware fallback. It does not change or restart the actual desktop service.
- A separate 1,920 × 1,080 Release replay renders 2,400 finite/lit frames with all
  GPU rows, zero warm C++/ImGui allocations and empty stderr. This dummy/muted
  correctness run is not timing, listening or whole-system memory acceptance.
- Both PowerShell orchestration suites pass with Conan on their child PATH.
  An initial missing-PATH setup failure is retained, not called a behavioral Red.
  These suites are not current-source native Windows compilation.

To exercise the service-dependent regression on an existing desktop:

```sh
env -u SDL_AUDIO_DRIVER -u SDL_AUDIODRIVER \
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 LD_BIND_NOW=1 \
  build/demo-asan/audio_output_tests '[desktop]'
```

`checks.json` records source/log hashes and evidence scope. Real coexistence uses
silent PCM, not a recording or subjective listening test. Sampling is not
continuous observation or arbitrary device-loss proof. The private Linux bundle
uses the platform `libpulse.so.0`, not the instrumented SDK client; sanitizer
success does not certify distro internals. Existing ALSA notices remain historical.
Same-session Core/C++ review found no remaining task-related defect; it is not an
independent review. Owner physical/nozzle acceptance, current Windows findings,
required UI refinement, final budgets and reviewed integration still block M3.
