# M3 final delivery candidates

The current Visual Identity 1.4 preview is private Linux v25, bound to signed
source `e45ba673040a348959cda48f95e9380a2e9f980e`. Launch
`build/m3-offline-linux-v25/ngen-flight/run.sh` without frame arguments for an
unlimited owner-closed preview. All 25 manifest hashes and 452 corresponding-
source members verify, including the supplied fonts/embedded notices and the
separate typeface notice. Both regular faces are embedded through the existing
ImGui atlas; no host font files or new runtime font/recording library is required.

The committed recorder was run with the v25 bundle, a fresh private output
`build/m3-identity14-recording-v25`, and `--seconds 20`. Its `recording.mkv` contains
19.498 s / 291 own-window frames, all retained in the mux, plus only NGEN's
48 kHz stereo application audio. No microphone/system mix; two owned playback
stream generations, nonzero unsaturated retained PCM and unchanged desktop audio
routing. Network isolation, hidden Conan and absent SDK environment are explicit.
A decoded frame was opened visually. Recording SHA-256:
`8233792025cfc5dc0e54c88839bddd6710d24e76855364bdf4822f7ccf0b81f6`.

This preview is **not fully verified for M3 acceptance**. The first instrumented
3,600-frame run reported 3,854 warm C++ calls / zero ImGui calls. Two debugger
recording comparisons reported eight; sampled stacks include planned reset and
post-loop driver/HDR-readback allocation paths but do not explain the original
increase. Counters were not suppressed; do not relabel that run as a clean budget
pass or attribute the difference to fonts. The full GUI-suite gesture failures
in [1.4 checks](../ui/brand14.md) also remain untraced despite focused passes.
[brand14-linux.json](brand14-linux.json) records these limits and exact identities.
Keep the PR draft; source/media correspondence does not supply final UI acceptance,
clean timing/memory proof, cross-distribution support, AV certification or legal
approval. Earlier hardware gates are not rerun for this theme revision.

The preceding Graphite 1.2 candidate is private Linux v24, bound to signed source
`27ba93914246bfa5f4fd13835a172c2f9482dc2e`. Launch
`build/m3-offline-linux-v24/ngen-flight/run.sh` for an unlimited owner-closed
preview. All 25 manifest hashes and 430 corresponding-source members verify,
including the replacement brand, recorder sources and protocol notice/license.
No Qt/KPipeWire recording SDK is linked into the NGEN executable.

The committed recording command was run locally:

```sh
python tools/record_flight.py --bundle build/m3-offline-linux-v24/ngen-flight \
  --output build/m3-graphite-recording-v24 --seconds 20
```

The private `build/m3-graphite-recording-v24/recording.mkv` contains 19.931 s /
266 frames of only the owned NGEN window, plus only its application audio.
All captured video frames survive muxing; the 48 kHz stereo FLAC input is
nonzero and unsaturated in the actual retained interval. Native playback monitor
selection follows two NGEN stream generations without a microphone, unrelated
application or whole-system mix. Networking is unshared, Conan hidden and SDK
environment absent; desktop audio routing/profiles remain unchanged. The bounded
3,600-frame instrumented walkthrough completes with all GPU rows and finite HDR.
A decoded recording frame was opened and visually inspected. No private media
is uploaded or committed.

Recording SHA-256:
`0bf4c024bb0e67705bf28d57f1310160a30c4c01976082d47c3a0e67372bd2ba`.
See [graphite-linux.json](graphite-linux.json) for manifest/executable identity
and scope. The recording walkthrough reports eight warm C++ calls and zero ImGui
calls, as did the old v23 pilot; the C++ call sites are untraced. Do not call this
allocation-free or clean timing/memory evidence. Audio/video alignment is
monotonic-time estimated, not sample-accurate certification. Earlier accepted
performance records remain at their actual revisions and are not rerun/relabelled.
The owner still needs to accept this revised UI; overall M3 acceptance and merge
remain blocked on that verdict, not another SDK/timing/memory rerun.

This v23 candidate predates the owner's replacement Graphite pack and remains
historical, not the revised-UI acceptance candidate. See
[Graphite verification](../ui/graphite.md).

The private Linux v23 bundle contains the GUI motion-burst repair and is bound to
signed source `c27f1ecf76dd950ab3cb1c37fff01a531831256d`. Launch it with
`build/m3-offline-linux-v23/ngen-flight/run.sh` without frame arguments for an
unlimited owner preview. Earlier bundles remain untouched.

All 25 manifest hashes verify; all 418 corresponding-source archive members are
admitted and file contents match the committed source. Brand generator inputs and
required notices/source are included. Manifest SHA-256:
`c6b8977b6040892f8780cab74d276eb1052d1ffd4d30a9fc8dc576a13ef2dcd2`.
Delivered executable SHA-256:
`05e19b84d87495ceaad2a3ec6090172989ab9266bcb68aa46790676e4e01ff01`.

A bounded 310-frame controls/data/performance/context walkthrough passes with
finite HDR and every GPU timing row. Networking is unshared, Conan is hidden and
SDK environment variables are absent. The muted client uses the existing desktop
Pulse service, not dummy audio or direct ALSA; card profiles, sinks and the default
sink remain unchanged. This is isolated startup/source correspondence on the
current Linux ABI, not cross-distribution or clean-machine proof. Details are in
[final-linux.json](final-linux.json).

The repaired Windows ClangCl bundle remains at source `5fb8ddf`; use the
[Windows handoff](../../../planning/m3-windows-handoff.md). Its validated runtime,
process-memory coverage and owner-confirmed allocation-free F2/F12 hover pass.
Three MSVC active-profile budget runs at `21e616c` also pass; their driver
variation and retained invalid trials are documented separately. Neither bundle
is relabeled with a later documentation revision. No accepted benchmark is rerun
for this packaging refresh.

Private assets, executables and recordings stay outside Git and public uploads.
The owner reports that real interactive disconnected startup and listening
passed. Overall acceptance remains withheld pending the new Graphite UI preview
and its private recording; earlier binaries are not relabelled. The integration
PR must remain a draft and must not be approved or merged before that acceptance. This
packaging verification is not milestone acceptance or redistribution/legal approval.
