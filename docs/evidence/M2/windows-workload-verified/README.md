# Verified Windows inspector/bloom workload

The owner returned genuine clean-tree execution at
`1f0a4b31a3db73f802513eab6edebd319f3c47cb` on the RTX 5070. MSVC and
clang-cl **Debug and Release each pass 10/10 CTests**, actual native resource/
material/shadow/IBL/bloom/overlay controls and the supplied canonical ship path.
The 1200-frame validation walkthrough in each configuration completes two
reloads, two resizes and generation 3. Ordinary workload stderr files are empty;
deliberate validation calibration remains separate. No build/failure workaround
was applied. This closes the changed-tree execution scope, **not formal M2,
independent graphics review or personal owner visual acceptance**.

| Release workload | MSVC | clang-cl |
|---|---:|---:|
| Steady frame P50/P95/P99 | 7.085/7.914/8.244 ms | 6.664/8.165/8.324 ms |
| Steady maximum | 8.595 ms | 8.496 ms |
| Four-reload frame P50/P95/P99 | 7.060/7.973/12.859 ms | 6.676/8.199/8.418 ms |
| Four-reload maximum | 24.372 ms | 12.840 ms |
| Observed streaming process working set | 924,434,432 bytes | 926,515,200 bytes |

Each optimized run has 60 warmup/600 measured frames, native 1280x720 throughout,
246,080 triangles, 100 genuinely instanced asteroids, 1024 shadow, exposure 1,
FXAA on and bloom 0.04. Post GPU intervals include fused bloom/tone/FXAA; UI is
separate. The steady paths observe zero C++ `new` and zero ImGui allocation calls.
Streaming intentionally allocates for replacement and reports those calls.
Percentiles above are independently recomputed from retained CSVs, not inferred
from a successful exit. All scoped timing/process-residency checks pass.

All four reloads reach generation 5. Settled tracked allocation/reservation return
to 199,302,016/202,240,192 bytes and 22 live/zero retiring records after each.
Tracked allocation peaks at 448,107,760 bytes during upload. These are **VMA**
figures, not whole-NVIDIA/WSI/driver memory. Working-set metadata is sampled OS
process residency/high-water observation, not GPU memory or an exact continuous
peak. Whole-process dedicated/shared GPU accounting and longer memory/quality
acceptance remain distinct gates. No new four-build rerun is requested for this
completed execution slice.

The input is the unchanged 175,854,711-byte canonical package with SHA-256
`e0ee6a023900c78b2c313f493f2a0e78c9639c39b6d830a1eaf3a3f6c0a29b6d`.
Every supplied artifact hash/size is checked against the returned BMP/CSV bytes.
Private BMPs are valid 1280x720 BGRA captures with equal 16-pixel outer gutters;
only hashes/metadata are retained, not pixels or model/package bytes. The
Windows fixture CRC is `a8bbdbce` in both compilers, versus Linux `740711bd`;
counts/seed/settings match, but cross-platform floating-point fixture bit identity
is not claimed. This does not alter the graphics-free M1 deterministic replay.

MSVC is 19.44.35229.0/toolset 14.44.35207. clang-cl is 19.1.5 targeting
x86_64-pc-windows-msvc, dynamic matching CRT/v144. The detected raw MSVC profile
says C++14; the actual invocation overrides host/build to C++20 and the native
configure logs explicitly confirm C++20/extensions off and matching Debug/Release
DLL runtime. Do not relabel that raw profile or infer the active mode from it.
SDK is 1.4.363.1 with locked 1.3.296 headers; driver raw 2588573696 decodes to
NVIDIA 617.42.0.0. Nine reflections retain the 32-byte post contract; native builds
verify PostDraw's explicit 80-byte ABI and offsets.

Archives: MSVC SHA-256
`96d3e60629a98acdfe3d11abaeac3086e64067228900fc05a8f52d07cca319db`;
clang-cl SHA-256
`0f630a0d0e6b1de0c53409b824abc862f21ee0dce4cbe3a9a84cbc2b8504a1da`.
Preflight bounds paths/types/counts, per-entry/total bytes and compression ratio;
85/86 selected files are extracted into fresh ignored directories and redacted-
scanned before inspection. Session, Conan/install, recipe-export and detection
setup logs are excluded and not read. Nothing extracted is executed.

Native JSON manifests/reflections/artifact/working-set records remain byte-exact
under narrow attributes; text/CSV line endings and public path displays are
normalized. Original and retained hashes are separate. `analysis.json` records
recomputed budgets and per-generation settlement. Hosted runs for the tested
source, bloom and gutter checkpoints are also green. The earlier Linux 256.8 ms
resize-acquisition tail and warm-file-cache scope remain documented, not erased
by these faster Windows results. Same-session evidence review is retained;
acceptance and integration are separate decisions.
