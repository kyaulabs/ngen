# M3 authoritative physical RCS

The v12 Linux bundle below is historical and retains the unsafe direct ALSA route.
Use the [desktop-audio replacement](../../audio-desktop/README.md) for new playback.
That audio repair preserves this checkpoint's physical replay and private asset.

The owner explicitly chose physical RCS: individual firings must drive the body,
and exhaust must show those same states. This supersedes the pulse-only and
post-physics directional checkpoints. Their exact old motion traces are historical,
not acceptance oracles for the changed physical model. See
[ADR 0017](../../../../adr/0017-physical-rcs-actuation.md).

The owner has now explicitly accepted v13's physical RCS handling, turn/coast/
opposing-arrest behavior and nozzle aim. Its actuation and directional geometry
are unchanged from this checkpoint. This is scoped flight/nozzle acceptance,
not approval of the remaining UI, Windows memory, budgets or M3 integration.

`Flight` now owns the copied physical rig, calibrated allocation and coordinated
valve state. Both control modes request a wrench through the existing controller.
Actual bank forces and position-cross-force moments, plus the separate forward
main drive, enter Jolt. Only after successful integration does the owning snapshot
publish `rcs_firing`, `main_drive_level` and actual force/torque. Invalid staged
commands do not advance valve state; prepare-before-swap reset clears it.

The renderer uses those published firing levels directly. The old application
pulse module and separate clock are removed. A small application adapter remains
only for static asset/calibration inspection and calls the same graphics-free
allocation function. Asset policy remains in the application; the game does not
acquire a renderer, GPU owner or asset decoder. Existing ideal continuous-actuator
fixtures remain valid without a physical RCS definition.

Duty follows normalized requested actuator authority, not the raw coefficient
relative to a nominal 1 GN bank rating. Otherwise full lateral/braking demand would
incorrectly become roughly one-third force. Lower demand uses bounded on-thrust
compensation; full configured demand may remain continuous. Actual bank levels,
body-local force/torque and speed limits scale the physical firings before Jolt,
never velocity after integration. Opposing couples can interrupt old off debt.
The 6-tick/3-tick timing, hysteresis and throttling are fictional NGEN actuator
policy, not NASA valve or propellant certification.

Public Reds reproduce smooth physical force without published burns, inherited
off debt delaying counterfire, inconsistent rig admission and loss of configured
force authority. They are Green. Sixteen pulse/physical cases independently
reconstruct force and moment from firing snapshots, exercise rotating mixed
actuation and speed admission, check real impulse/velocity integration, true quiet
coast, copied definitions, reset, main-drive isolation and continuous saturated
lateral authority. The accepted steering targets still pass: at least 0.35 rad/s
within 1 s and release below 0.04 rad/s within 0.3 s.

Physical float-calibration cancellation residuals are integrated, not silently
zeroed. Pure-turn and focus tests bound resulting linear drift to 0.0001 m/s.
The SDL checker keeps braking below 1 m/s at the original check point, accepts real
off/coast intervals rather than demanding positive force every tick, and rejects
resumed linear intent. The examined focus interval had a 95.7 N maximum residual
force and a 0.000000537 m/s maximum velocity change on the 12,000,000 kg hull.
`check-walkthrough.py` preserves the current physical assertions; earlier failing
raw/checker traces remain private, separate from behavioral Reds.

Eight complete native graphs pass: four demo graphs with 22 CTests each and four
headless flight graphs with 8 each, 120 CTests total. Sixty actual Clang command
analyses cover header consumers and distinct Debug/Release/ASan configurations;
affected commands were rerun after real fixes. No detector was suppressed: typed
float constants and explicit guarded test fixtures fixed narrowing/optional
findings. GCC native tests pass; its incompatible AST flags are not stripped.
Twelve native RCS cases and four SDL/control/audio/reset walkthroughs pass.

The 2,400-frame 1920 × 1080 physical replay has finite/lit HDR, all GPU timing rows,
monotonic host clocks, empty stderr and 0 C++/0 ImGui warm allocations. All-actuators-
off rows have exactly zero actuation. The report remains 63 columns under the
unchanged 64-column observer limit. It does not claim old trajectory equality.
Muted dummy audio and VMA counters are not listening, final timing or process/global
GPU-memory acceptance. `checks.json` binds current results to source hashes.

The private 32-jet asset is unchanged from the directional checkpoint:
`build/cooked-candidate/frigate-rcs-directional-v1/scene.ngas`, SHA-256
`99c996f33d3cc98dbd801e3aa302ee2a5894d0fa6bcfdd18f59b73e72e74d834`, CRC marker
`6bbe6157`. Native verification independently matches physical moment arms and
thrust directions to the immutable rendered nozzles. Workload remains 249,942
triangles/247 ship instances; the material ABI remains 64 bytes. Obsolete profile
padding was removed after the larger owning rig changed native layout; Windows
MSVC/ClangCl must still verify the new layout.

The private Linux v12 bundle is `build/m3-offline-linux-v12/ngen-flight`, from
signed behavior source `4aa217c41bc447abc98ca90435818bc6131e614e`. All 23 manifest
hashes and the bounded 244-member source archive validate, including the moved
simulation actuator sources and removal of the old application module. Headless
physical-geometry checks and three rendered frames pass with networking unshared,
Conan hidden and SDK/library-path overrides absent; see `offline.json`. Platform
ABI and final Windows/UI/budget qualifications still apply.

The actual interactive bundle was opened for owner review, with the accepted
tactical-orbit startup and ordinary direct/tactical controls. It is not the old
fixed-hull or scripted firing inspection. It has no frame/time limit, automatic
closure, recording sidecar or microphone capture. Owner closes the window.

Same-session review traced physical authority, copied ownership, admission,
quaternion interval, staged publication/reset, compensation/limits, main-drive
separation, numerical residuals and source/header consumers. It found no remaining
actionable defect in this slice; it is not independent review.

Owner physical-gameplay/nozzle-aim acceptance, current Windows/RTX and historical
CIM/ImGui findings, deferred UI refinement, final affected budgets/delivery and
reviewed integration remain open. M3 is not complete.
