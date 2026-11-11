# M3 Windows flight check

Use a clean checkout of `feat/kyau-023ba5-m3-flight`. Set `$scene` to the existing
accepted cooked package, not the GLB. Its SHA256 is
`a02d5438ad32d9c8fb917ce847d5a822688ca5b51d69125f6da71cf2ba2116ed`.
This checks M3; it does not reopen accepted M2 appearance or request new art approval.

Run in PowerShell7.2 or newer. Use the named splat; do not split `Start-Process`
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
whole build directory. They contain current-source build/test results, a330-frame
validation/control/audio walkthrough, a separate unvalidated2400-frame1080p replay,
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
M3 remains open until these checks, laptop/RTX budgets and reviewed integration finish.
