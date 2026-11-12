# M3 Windows flight check

Use a clean checkout of `feat/kyau-023ba5-m3-flight`. Set `$scene` to the new
private RCS cooked derivative, not the GLB or original M2 package. Its SHA-256 is
`99c996f33d3cc98dbd801e3aa302ee2a5894d0fa6bcfdd18f59b73e72e74d834`;
its CRC compatibility marker is `6bbe6157`. Copy it to a new path and preserve the
accepted M2 original. The owner-requested RCS addition needs visual review; the
accepted rear exhaust/hull appearance is not reopened. See the
[directional 32-jet RCS checkpoint](../evidence/M3/rcs/directional/README.md).
The current private input is `build/cooked-candidate/frigate-rcs-directional-v1/scene.ngas`;
older all-bank firing candidates are not equivalent to this source/profile.

Run in PowerShell 7.2 or newer. Use the named splat; do not split `Start-Process`
arguments or use positional `Select-String` examples.

```powershell
$check = @{
    Configuration = 'All'
    Compiler = 'MSVC'
    FlightDemo = $true
    ScenePackage = $scene
}
& ./tools/build-windows.ps1 @check
```

After it finishes successfully, run the second compiler sequentially:

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
M3 remains open until these checks, RCS owner review, the required deferred UI pass,
laptop/RTX budgets and reviewed integration finish.
