#Requires -Version 7.2
[CmdletBinding()]
param([string]$RunDirectory = '')

if (-not $IsWindows) {
    [Console]::Error.WriteLine('This flight continuation requires Windows and PowerShell 7.2+.')
    exit 2
}
. (Join-Path $PSScriptRoot 'build-windows.ps1')

& {
    $ErrorActionPreference = 'Stop'
    $PSNativeCommandUseErrorActionPreference = $false
    [string]$root = Split-Path -Parent $PSScriptRoot
    $selection = @{ SourceRoot = $root; Compiler = 'ClangCl'
        RunDirectory = $RunDirectory }
    $run = Get-NgenWindowsFlightRun @selection
    Write-Host "Continuing ClangCl Release batch: $run"
    $id = [guid]::NewGuid().ToString('N')
    [string]$logs = Join-Path $root "build\clang-release-resume-$id"
    New-Item -ItemType Directory -Path $logs | Out-Null
    $scriptCode = 0
    Push-Location -LiteralPath $root
    try {
        Enter-NgenDeveloperShell
        $sdkArguments = @{
            ProcessRoot = $env:VULKAN_SDK
            UserRoot = [Environment]::GetEnvironmentVariable(
                'VULKAN_SDK', 'User'
            )
            MachineRoot = [Environment]::GetEnvironmentVariable(
                'VULKAN_SDK', 'Machine'
            )
        }
        $env:VULKAN_SDK = Get-NgenVulkanSdkRoot @sdkArguments
        [string]$build = Join-Path $run 'Release'
        $toolDirectory = Join-Path $root 'build\windows-tools\Scripts'
        [string]$cmake = Join-Path $toolDirectory 'cmake.exe'
        [string]$ctest = Join-Path $toolDirectory 'ctest.exe'
        [string]$python = Join-Path $toolDirectory 'python.exe'
        $assetSelection = @{ SourceRoot = $root; Compiler = 'MSVC'
            RequireBundle = $true }
        $assetRun = Get-NgenWindowsFlightRun @assetSelection
        [string]$asset = Join-Path $assetRun (
            'offline\ngen-flight\assets\ship.ngas'
        )
        $assetDirectory = Get-Item -LiteralPath (Split-Path -Parent $asset) -Force
        if ($assetDirectory -isnot [IO.DirectoryInfo] -or
            ($assetDirectory.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'The accepted asset directory must not be a link.'
        }
        $assetIdentity = Get-NgenSceneIdentity -Path $asset
        if ($assetIdentity.sha256 -cne (
            '99c996f33d3cc98dbd801e3aa302ee2a5894d0fa6bcfdd18f59b73e72e74d834'
        )) { throw 'The selected MSVC bundle contains a different asset.' }
        $metadata = @(
            'host-profile.txt', 'build-profile.txt',
            'clang-cl-version.txt', 'Release-configure.txt'
        )
        foreach ($name in $metadata) {
            $old = Join-Path (Join-Path $run 'logs') $name
            $item = Get-Item -LiteralPath $old -Force
            if ($item -isnot [IO.FileInfo] -or
                ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
                $item.Length -lt 1 -or $item.Length -gt 2097152) {
                throw 'Evidence metadata is linked, nonregular or oversized.'
            }
            Copy-Item -LiteralPath $old -Destination $logs
        }
        Invoke-NgenNative -FilePath 'git' -Arguments @(
            'rev-parse', 'HEAD'
        ) -LogPath (Join-Path $logs 'revision.txt')
        Invoke-NgenNative -FilePath 'git' -Arguments @(
            'status', '--short'
        ) -LogPath (Join-Path $logs 'working-tree.txt')
        Invoke-NgenNative -FilePath $cmake -Arguments @(
            '--build', $build, '-j', '2', '--', '-k', '0'
        ) -LogPath (Join-Path $logs 'Release-build.txt')
        $checks = {
            Invoke-NgenNative -FilePath $ctest -Arguments @(
                '--test-dir', $build, '--show-only'
            ) -LogPath (Join-Path $logs 'Release-test-list.txt')
            Invoke-NgenNative -FilePath $ctest -Arguments @(
                '--test-dir', $build, '--output-on-failure',
                '--no-tests=error'
            ) -LogPath (Join-Path $logs 'Release-tests.txt')
            $harness = Join-Path $build 'sim_harness.exe'
            $serial = @{
                FilePath = $harness
                Arguments = @()
                LogPath = Join-Path $logs 'Release-serial.txt'
            }
            Invoke-NgenNative @serial
            Invoke-NgenNative -FilePath $harness -Arguments @(
                '--workers=2'
            ) -LogPath (Join-Path $logs 'Release-workers.txt')
        }
        $common = @{
            BuildDirectory = $build
            LogDirectory = $logs
            Configuration = 'Release'
        }
        Invoke-NgenRenderer @common -Checks $checks
        Read-Host 'Next: flight replays. Keep the demo foreground' | Out-Null
        $bundleOutput = $logs + '-offline'
        $flight = @{
            ScenePackage = $asset
            Python = $python
            SourceRoot = $root
            BundleOutput = $bundleOutput
        }
        Invoke-NgenFlight @common @flight
        $bundle = Join-Path $bundleOutput 'ngen-flight'
        [string]$binary = Join-Path $bundle 'flight_demo.exe'
        [string]$localAsset = Join-Path $bundle 'assets\ship.ngas'
        Write-Host 'Next is a bounded hover diagnostic, not a preview.'
        Write-Host 'After 3 s, use F2 plot tooltips with numeric F12 live.'
        Read-Host 'Press Enter when ready; let it close itself' | Out-Null
        [string]$prefix = Join-Path $logs 'Release-flight.hovered-live'
        [string[]]$hoverArguments = @(
            $localAsset, '--frames=3600', '--warmup=60',
            '--width=1920', '--height=1080',
            "--report=$prefix.csv", "--capture=$prefix.bmp"
        )
        $stdout = "$prefix.txt"
        $stderr = "$prefix.stderr.txt"
        & $binary @hoverArguments 1> $stdout 2> $stderr
        if ($LASTEXITCODE -ne 0) { throw 'Hover diagnostic failed.' }
        [string[]]$files = @(
            $stdout, $stderr, "$prefix.csv", "$prefix.bmp"
        )
        Get-FileHash -LiteralPath $files -Algorithm SHA256 |
            Select-Object Path, Hash | ConvertTo-Json |
            Set-Content -LiteralPath (Join-Path $logs 'hover-artifacts.json')
    } catch {
        $scriptCode = 1
        $_ | Out-String |
            Set-Content -LiteralPath (Join-Path $logs 'failure.txt')
    } finally {
        try {
            [ordered]@{
                resumed_batch = Split-Path -Leaf $run
                configuration = 'Release'
                configure_metadata_reused = $true
                debug_and_msvc_repeated = $false
            } | ConvertTo-Json |
                Set-Content -LiteralPath (Join-Path $logs 'resume.json')
            [string]$archive = $logs + '.zip'
            Compress-Archive -Path (Join-Path $logs '*') -DestinationPath $archive
            Write-Host "SEND CLANG RELEASE: $archive"
        } finally { Pop-Location }
    }
    exit $scriptCode
}
