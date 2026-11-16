#Requires -Version 7.2
[CmdletBinding()]
param([string]$RunDirectory = '')

# Two timed verification replays, not an interactive preview. No build or recorder.
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
if (-not $IsWindows) {
    [Console]::Error.WriteLine('These flight replays require Windows and PowerShell 7.2+.')
    exit 2
}
. (Join-Path $PSScriptRoot 'build-windows.ps1')
[string]$root = Split-Path -Parent $PSScriptRoot
$selection = @{ SourceRoot = $root; Compiler = 'MSVC'
    RunDirectory = $RunDirectory; RequireBundle = $true }
$run = Get-NgenWindowsFlightRun @selection
Write-Host "MSVC replay batch: $run"
[string]$bundle = Join-Path $run 'offline/ngen-flight'
[string]$binary = Join-Path $bundle 'flight_demo.exe'
[string]$asset = Join-Path $bundle 'assets\ship.ngas'
$assetDirectory = Get-Item -LiteralPath (Join-Path $bundle 'assets') -Force
if ($assetDirectory -isnot [IO.DirectoryInfo] -or
    ($assetDirectory.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
    throw 'The accepted asset directory must not be a link.'
}
foreach ($path in @($binary, $asset)) {
    $item = Get-Item -LiteralPath $path -Force
    $limit = if ($path -eq $asset) { 268435456 } else { 134217728 }
    if ($item -isnot [IO.FileInfo] -or
        ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
        $item.Length -lt 1 -or $item.Length -gt $limit) {
        throw 'Existing replay input is linked, nonregular or oversized.'
    }
}
$manifest = Get-Content -LiteralPath (Join-Path $bundle 'manifest.json') -Raw | ConvertFrom-Json
if ($manifest.schema -ne 1 -or $manifest.platform -cne 'Windows' -or
    $manifest.source_revision -cnotmatch '^[0-9a-f]{40}$' -or
    (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash -ine $manifest.files.'flight_demo.exe' -or
    (Get-FileHash -LiteralPath $asset -Algorithm SHA256).Hash -ine '99c996f33d3cc98dbd801e3aa302ee2a5894d0fa6bcfdd18f59b73e72e74d834') { throw 'Existing MSVC bundle identity mismatch.' }
[string]$out = Join-Path $root ('build\msvc-focused-replays-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
$scriptCode = 0
Push-Location -LiteralPath $root
try {
    foreach ($index in 2, 3) {
        Read-Host "Ready for replay $index? Press Enter, then keep the demo foreground without input until it closes" | Out-Null
        [string]$prefix = Join-Path $out "focused-$index"
        [string[]]$arguments = @($asset, '--replay', '--frames=2400', '--warmup=60', '--width=1920', '--height=1080', "--report=$prefix.csv")
        & $binary @arguments 1> "$prefix.txt" 2> "$prefix.stderr.txt"
        if ($LASTEXITCODE -ne 0) { throw "Replay $index failed; retain its output." }
    }
} catch {
    $scriptCode = 1
    $_ | Out-String | Set-Content -LiteralPath (Join-Path $out 'failure.txt')
} finally {
    try {
        [ordered]@{ source_revision = $manifest.source_revision
            manifest_sha256 = (Get-FileHash -LiteralPath (Join-Path $bundle 'manifest.json') -Algorithm SHA256).Hash
            executable_sha256 = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash
            asset_sha256 = (Get-FileHash -LiteralPath $asset -Algorithm SHA256).Hash
            scope = 'Existing MSVC bundle, two clean direct-desktop replay repeats; all rows retained. No external memory sampling.' } |
            ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'identity.json')
        [string[]]$files = @(Get-ChildItem -LiteralPath $out -File | ForEach-Object { $_.FullName })
        Get-FileHash -LiteralPath $files -Algorithm SHA256 |
            Select-Object Path, Hash | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $out 'artifacts.json')
        [string]$archive = $out + '.zip'
        Compress-Archive -Path (Join-Path $out '*') -DestinationPath $archive
        Write-Host "SEND FOCUSED REPLAYS: $archive"
    } finally { Pop-Location }
}
exit $scriptCode
