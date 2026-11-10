# Windows continuity-fix verification

The [numeric return and local regression](lattice-continuity/README.md) identify
both an R16F plateau that defeated the old one-sided estimate and a separate
lattice-boundary spike exposed by the stronger check. The fix shares one lattice
origin between hashing/interpolation and checks both boundary steps against the
two outer steps. The multiplier2 and absolute1/1024 allowance stay unchanged.
The shader arithmetic changes; materials, authored noise/tail parameters, ABI,
bindings, private package and accepted art settings do not.

Use the existing build workflow for **one fresh MSVC Release** graph. Do not reuse
the earlier executable: its diagnostic-marker check was false despite updated Git
source. Earlier manual capture also split `-RedirectStandardError` into a command;
a positional `Select-String` example was wrong. The named parameter splats below
avoid both command-line binding/wrapping errors. No additional collector, old
compiler matrix, recook, WDDM measurement or visual approval is required.

In PowerShell7 at `D:\Git\kyaulabs\ngen`, run the entire block:

```powershell
& {
    $ErrorActionPreference = 'Stop'
    $tree = @(git status --porcelain)
    if ($LASTEXITCODE -ne 0 -or $tree.Count -ne 0) {
        throw 'Stop: preserve local edits or fix the repository path.'
    }
    git switch feat/kyau-ade38c-m2-acceptance
    if ($LASTEXITCODE -ne 0) { throw 'Branch switch failed' }
    git pull --ff-only origin feat/kyau-ade38c-m2-acceptance
    if ($LASTEXITCODE -ne 0) { throw 'Source update failed' }

    $check = @{
        LiteralPath = '.\apps\resource_probe\main.cpp'
        Pattern     = 'emissive_flow_continuity_calibration=1'
        SimpleMatch = $true
        Quiet       = $true
    }
    if (-not (Select-String @check)) {
        throw 'Continuity-fix source is missing; do not build.'
    }
    $options = @{
        Compiler      = 'MSVC'
        Configuration = 'Release'
        Inspector     = $true
        ScenePackage  = '.\build\m2-final-visual-input\scene.ngas'
    }
    & .\tools\build-windows.ps1 @options
}
```

The already-correct package remains194,096,492bytes/SHA256
`a02d5438ad32d9c8fb917ce847d5a822688ca5b51d69125f6da71cf2ba2116ed`.
The normal script records its input identity and archives the native output even
if the probe fails. `Restoring environment` plus its exit1 exception is ordinary
cleanup/reporting of that probe result, not a separate engine failure.

Return the new `evidence.zip` printed by this invocation, even on failure. Expect
`emissive_flow_continuity_calibration=1`, all existing resource/inspector controls,
and the final-package workloads if native checks pass. On failure, preserve the
bounded `wrap_sample` and actual revision. Do not infer Windows success from the
local Iris runs. M2 remains open until current Windows execution and normal
reviewed integration complete; M3 has not started.
