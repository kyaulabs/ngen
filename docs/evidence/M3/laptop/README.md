# Final-source laptop and owner checks

Behavior source: signed `95645f6deb565ca05d4c820f8c695b726f7fe38a`.
Exact-head hosted CI43077400206 passes4/4. This checkpoint is local M3 evidence,
not Windows/RTX acceptance or completed milestone integration.

Three sequential runs use the private v7 Release bundle, immutable accepted asset
SHA256 `a02d5438ad32d9c8fb917ce847d5a822688ca5b51d69125f6da71cf2ba2116ed`,
1280×720,247542 triangles,100 asteroid instances/contacts,1024 shadow map and the
same2400-tick input/camera replay. Each run prewarms600 frames, then measures every
one of2400 frames. No streaming frame, tail or outlier is exempted. The owner kept
each window foreground without manual controls; every row reports focus and actual
playing audio. No build, scan or recording intentionally ran alongside timing.
Validation/sanitizers are off; the allocation probe is on. Whole application frame
cost ends at queued presentation/collection, not proven display scanout latency.

| Run | Median ms | P95 ms | P99 ms | Maximum ms | Kernel peak RSS bytes |
|---|---:|---:|---:|---:|---:|
|1|16.690713|18.084058|18.525526|19.185600|394747904|
|2|16.676801|18.473046|19.119536|20.984758|394477568|
|3|16.668893|18.081974|18.572567|19.109168|394518528|

All B1 timing and resident-set budgets pass; fixed physical/camera traces match
between runs. Measured C++/ImGui allocation counts are0/0. These counters do not
cover C/driver heaps. Kernel `wait4` peak RSS is per owned process across startup
and playback, not Windows working set. VMA-owned allocation/reservation maxima are
232000808/234698712 bytes in every run, below the Context512MiB envelope and1GiB
owned-resource target. Texture allocations are a subset of that owned total; this
is not hardware cache residency, global GPU pressure or a continuous process-GPU
observation. Do not add VMA and RSS on UMA. Raw CSVs and per-run read-only power,
thermal, clock and cgroup snapshots are retained; snapshots are not continuous
thermal/clock proof and no system power settings were changed by the agent.

Current device/driver/CPU/kernel/toolchain/lock and binary identities accompany the
results. This is physical Intel Iris Xe/Mesa Wayland, not software Vulkan, RTX or a
fabricated GTX VM. Earlier source660c9dd background runs were unfocused/muted and
missed the50ms median limit (50.6–50.8ms); they were not accepted or relabeled. Their
summary and raw-sample hashes remain. Later source660c9dd focused runs passed, but
only the final-source runs above support this checkpoint.

The private v7 bundle's entire file manifest and bounded regular source archive
validate. Archived behavior files match this source; networking-unshared startup
with Conan hidden and SDK variables absent renders finite HDR/all GPU rows with
empty stderr. Linux platform ABI requirements and licensing/source/relinking
limitations remain as documented; this is not arbitrary clean-distribution proof.

Owner confirmations after hands-on work: translational braking is perfect; revised
steering needed a5–10% trim, and the final7.5% trim feels right; stale-autopilot TAB
handoff is fixed; listening is good but quiet; M works; an actual playback-device
reconnection recovers after refocus while flight continues. Plain recovery ended
playing with generation1/losses0 and empty stderr: the platform route may recover
without an SDL stream retirement, so this is not proof of that native callback path.
Synthetic retirement/reopen has its separate recorded tests.

The owner saved a post-fix private VP9 recording. FFmpeg combines it with this
application's captured native PCM; no microphone/other application audio is used.
Originals remain unchanged. Six actual TAB/UI-tone pairs align with≤26ms residual;
the resulting60.165s clip fully decodes, with original video packets preserved.
This alignment does not measure audio-device latency. Media bytes and preview
frames stay outside Git; `private-media-metadata.json` records hashes/scope only.
Human recording clients have no frame limit or automatic termination: the owner
stops/saves the recorder and closes the client themselves.

Remaining: current Windows MSVC/ClangCl compatibility, app-local offline loader
startup and scoped RTX1920×1080 timing/memory; final review, reviewed PR integration
and exact post-merge CI. No duplicate M2 art approval is requested.
