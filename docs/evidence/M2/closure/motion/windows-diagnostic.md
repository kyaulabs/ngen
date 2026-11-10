# Focused RTX wrap diagnostic

Use the **same PowerShell7 terminal** that just built
`build\windows\20261110-094948-812\Release`, so its MSVC environment is still
active. Update the work branch, then rebuild/run only `resource_probe` against
that existing Release graph. No new matrix, full workload, asset, driver or
memory collection is needed.

The patch adds one bounded raw/linear sample on failure. It does not change the
shader, accepted art, half decoder or continuity threshold and is not a claimed
NVIDIA repair. A nonzero probe exit is preserved in the log.

```powershell
$ErrorActionPreference = 'Stop'
$tree = @(git status --porcelain)
if ($LASTEXITCODE -ne 0 -or $tree.Count -ne 0) { throw 'Stop: preserve local edits.' }
git switch feat/kyau-ade38c-m2-acceptance
if ($LASTEXITCODE -ne 0) { throw 'Branch switch failed' }
git pull --ff-only origin feat/kyau-ade38c-m2-acceptance
if ($LASTEXITCODE -ne 0) { throw 'Source update failed' }
if (-not (Select-String -LiteralPath .\apps\resource_probe\main.cpp -SimpleMatch 'wrap_sample x=')) { throw 'Diagnostic source is missing' }
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) { throw 'Use the same terminal that built Release; MSVC is not active.' }

$build = Join-Path $PWD 'build\windows\20261110-094948-812\Release'
$log = Join-Path $PWD ('build\rtx-wrap-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.txt')
$revision = git rev-parse HEAD
if ($LASTEXITCODE -ne 0) { throw 'Could not record source revision' }
if (Test-Path -LiteralPath $log) { throw 'Diagnostic output already exists' }
Set-Content -LiteralPath $log -Value "revision=$revision"
& .\build\windows-tools\Scripts\cmake.exe --build $build --target resource_probe -j 2 2>&1 | Tee-Object -FilePath $log -Append
if ($LASTEXITCODE -ne 0) { throw "Probe rebuild failed; send $log" }

$nativePreference = $PSNativeCommandUseErrorActionPreference
try {
    $PSNativeCommandUseErrorActionPreference = $false
    . (Join-Path $build 'conanrun.ps1')
    $hash = (Get-FileHash (Join-Path $build 'resource_probe.exe') -Algorithm SHA256).Hash
    Add-Content -LiteralPath $log -Value "probe_sha256=$hash"
    & (Join-Path $build 'resource_probe.exe') 2>&1 | Tee-Object -FilePath $log -Append
    $probeExit = $LASTEXITCODE
    Add-Content -LiteralPath $log -Value "probe_exit=$probeExit"
} finally {
    . (Join-Path $build 'deactivate_conanrun.ps1')
    $PSNativeCommandUseErrorActionPreference = $nativePreference
}
Write-Host "Send this diagnostic file: $log"
```

Send that single text file, not another full build archive. The failure sample
provides coordinate/channel, raw R16F words and radiance at phase ticks−2/−1/0/+1/+2,
plus the unchanged predicate values. That distinguishes numeric representation/
local curvature from a real seam before a fix is selected. Do not supply PINs or
credentials, change execution policy or mark the failure waived.
