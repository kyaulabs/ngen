# M3 demand-driven RCS puffs

The owner accepts every location in the 32-jet rig and requests pulsed firing.
This change preserves the approved private asset, mounts, plume geometry/materials,
main rear exhaust, physical controller and audio policy. It changes presentation
firing only, not the applied physical wrench or ship response.

NASA's Hall et al., *Design and Stability of an On-Orbit Attitude Control System
Using Reaction Control Thrusters* (2016), describes constant-thrust on/off RCS,
deadzones/hysteresis to reduce chatter, jet selection and ideal bang–off–bang
maneuvers: accelerate rotation, coast, then oppose it to stop. Disturbances can
require additional corrections. Sections II and IV–VI support those principles;
[NASA's full text](https://ntrs.nasa.gov/api/citations/20160001829/downloads/20160001829.txt)
and [citation record](https://ntrs.nasa.gov/citations/20160001829) are public primary
sources. NASA's [GNC survey, section 5.2.4](https://www.nasa.gov/smallsat-institute/sst-soa/guidance-navigation-and-control/)
also identifies minimum impulse bit as a translation/pointing precision constraint.
These are not specifications for NGEN's fictional thruster hardware.

`RcsPulses` is an owning, bounded application value. Each successful 60 Hz game
tick supplies the existing calibrated bank demands from the actual world wrench
and preceding attitude. A common gate preserves complete force/moment patterns;
render reads only copy the cached output. There is no random flashing, per-frame
clock, new renderer resource or material ABI change.

Small corrections borrow a short visible burst, then repay a bounded visual duty
budget while off. The budget uses the existing fourth-root perceptual response;
on-burst opacity/emission remain unchanged. Entry/exit demand thresholds of
0.0002/0.0001 suppress faint residual chatter. Sustained partial demand uses at
least 6 ticks on and 3 ticks off: 100 ms and 50 ms. Saturated unit demand may remain
on continuously. Idle/reset cancels immediately, fresh demand responds promptly,
and saturation can override an off interval. Minimum durations apply while demand
persists; this is not a simulated physical valve/minimum-impulse-bit model.
Those thresholds/timings are NGEN presentation choices, not NASA hardware values.

Public runtime Reds reproduced constant low-demand exhaust, residual chatter,
missing minimum burn after saturated demand eases, and no quiet correction frames
in the actual rendered replay. All are Green. Nine pulse cases cover coordinated
puffs, minimum durations, saturation, coast, hysteresis, atomic invalid admission,
reset/copy ownership, render-cadence independence and actual three-axis release
arrest. An initial unused-private-field compilation failure is retained separately
and is not counted as a behavioral Red; no warning was disabled.

Four complete native graphs pass 22 CTests each: Clang Debug/Release, strict
ASan+UBSan and GCC. Nine actual-command Clang analyses cover the three affected
implementation/header consumers across three configurations. Twelve opt-in native
RCS cases pass. GCC AST remains unavailable because unsupported flags are retained.

The 2,400-frame 1920 × 1080 Release replay has finite output, all GPU rows, monotonic
host clocks, empty stderr and 0 C++/0 ImGui warm allocations. It contains 532 firing
frames and 540 quiet frames with correction demand above the entry threshold.
All 24 physical/rear-power fields match the original-asset baseline exactly.
The CSV now distinguishes visible `rcs_power` from `rcs_demand`; 63 columns remain
inside the unchanged 64-column Windows observer bound. Muted dummy-backend replay
is correctness/allocation evidence, not listening, final timing or Windows memory
acceptance. `checks.json` binds results to source hashes.

The private Linux v10 bundle is `build/m3-offline-linux-v10/ngen-flight`, from
signed behavior source `8968947fb9ecbb17bdc04e6bb462392e05980b70`. All 23 manifest
hashes and the bounded 244-member source archive validate, including the new pulse
implementation/header/test/build sources. Headless verification and three rendered
frames pass with networking unshared, Conan hidden and SDK/library-path overrides
absent. See `offline.json`; platform ABI and final UI/Windows/budget qualifications
remain unchanged.

Same-session review traced pulse state, saturation transitions, budget bounds,
invalid atomicity, tick/render separation, reset and borrowed material-table
lifetimes. The verified burst transition repair is included. This is not independent
review or a spacecraft control/propellant certification.

The all-on coverage client remains a separate location-inspection tool. The new
owner-closed pulse inspection uses this production scheduler with labelled gentle
correction, saturated, coast and all-bank timing fixtures; it is not a physics
replay. No human preview has an automatic lifetime or recording sidecar.

Owner pulse-timing approval, current Windows/RTX checks and the existing CIM/ImGui
findings remain open. The required deferred UI pass, final affected delivery and
budgets, review and normal integration still gate M3 completion.
