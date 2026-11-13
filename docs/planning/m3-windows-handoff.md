# M3 Windows flight check

Use a clean checkout of `feat/kyau-023ba5-m3-flight`. Set `$scene` to the new
private RCS cooked derivative, not the GLB or original M2 package. Its SHA-256 is
`99c996f33d3cc98dbd801e3aa302ee2a5894d0fa6bcfdd18f59b73e72e74d834`;
its CRC compatibility marker is `6bbe6157`. Copy it to a new path and preserve the
accepted M2 original. The owner accepts the current physical RCS handling/nozzle
aim; the accepted rear exhaust/hull appearance is not reopened. See the
[directional 32-jet RCS checkpoint](../evidence/M3/rcs/directional/README.md).
The current private input is `build/cooked-candidate/frigate-rcs-directional-v1/scene.ngas`;
older all-bank firing candidates are not equivalent to this source/profile.
The [physical RCS simulation](../evidence/M3/rcs/physical/README.md) now applies
published bank firings to Jolt; rebuild the current source, including its changed
owning ship/snapshot layout. Earlier smooth-wrench trajectories are historical,
not current physical replay acceptance.

First isolate the recurring MSVC memory-observer timeout without another full
build. Start an **already built** native Windows flight demo normally, wait for
the scene to load, and leave it open. In a second PowerShell 7.2+ terminal, list
only these clients and choose the ID of the window you want to inspect:

```powershell
Get-Process -Name flight_demo | Select-Object Id, StartTime, MainWindowTitle
$probe = @{ DiagnoseGpuMemoryProcessId = 12345 } # Replace with that actual ID.
& ./tools/build-windows.ps1 @probe
```

Send the small `evidence.zip` path printed by this diagnostic. It does not build,
install anything, start/stop the client, alter services/counters or elevate. Close
the client yourself when finished. PID/start-time checks stop collection if the
selected process exits or changes.

The read-only sequence is formatted → raw → formatted → raw, with the same
owned-PID filter, fields and 2 s CIM operation timeout. Each attempt retains its
elapsed time, projected counter values and bounded typed error details; failures
do not hide the other attempts. This tests whether the failure is shared by the
raw provider or specific to formatted/cold queries. A pattern alone does not prove
the underlying cause, and raw readings are not substituted for budget acceptance.
The CIM timeout is not a hard OS wall-clock guarantee. Do not run the diagnostic
alongside a clean timing or formal memory measurement.

After diagnosis and any verified repair, the current-source Windows checks below
remain necessary for the tooltip fix and F12 panel, including actual dedicated-VRAM
readings. Use the named splat; do not split `Start-Process` arguments or use
positional `Select-String` examples.

```powershell
$check = @{
    Configuration = 'All'
    Compiler = 'MSVC'
    FlightDemo = $true
    ScenePackage = $scene
}
& ./tools/build-windows.ps1 @check
```

After that run exits, preserve its evidence archive and run the second compiler
sequentially, even if the first run failed. Do not change profiles or suppress a
failed check to reach the other compiler:

```powershell
$check.Compiler = 'ClangCl'
& ./tools/build-windows.ps1 @check
```

Each run prints its `evidence.zip` path. Send those two archives privately, not the
whole build directory. They contain current-source build/test results, a 330-frame
validation/control/audio walkthrough, a separate unvalidated 2,400-frame 1080p replay,
and another replay with sampled working-set/WDDM observations. The latter is
instrumented memory evidence, not clean timing or a continuous global-memory bound.
No GTX VM result is implied.

The historical ClangCl return reported 2 warm ImGui allocations in the clean
replay and a first-query CIM failure with no GPU-memory samples. These findings
remain open until a current-source run establishes the affected behavior. The
observer now retains `error_details` in `Release-flight.memory.working-set.json`:
PowerShell error identifier/category and, for an actual CIM exception, its native
error name and numeric status. The 2 s operation timeout, sampling/coverage rules
and budgets are unchanged. A failed query still fails memory acceptance; do not
restart services, rebuild counters, elevate permissions or relax checks on the
basis of the historical generic message.

The Release private bundle is beside that run's build directories at
`offline/ngen-flight`. The workflow checks headless admission and three rendered
frames with Conan deactivated and SDK paths removed. It does **not** disconnect
networking or change firewall/adapter settings. Disconnect networking yourself
and launch `run.cmd` to confirm the interactive offline path. Do not publish or
send the bundle: it contains the private accepted asset.

Try tactical orbit, TAB to the authored direct camera, thrust then coast, SPACE
braking, ALT+RMB free look, focus changes and reset. Check that switching modes
preserves motion, release/OS focus loss clears controls, and pointer capture exits
cleanly. Listen to engine/thrust/brake/UI cues; try M, real focus loss and a real
playback-device removal/reconnection where available. Report control feel, sound
comfort and any discontinuity. Synthetic SDL events are not substitutes for this.
Keep a short original audio/video recording private; send its hash and feedback.
M3 remains open until these Windows checks, the required deferred UI pass,
final laptop/RTX budgets, owner acceptance and reviewed integration finish.
