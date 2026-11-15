#Requires -Version 7.2
[CmdletBinding()]
param([string]$RunDirectory = '')

if (-not $IsWindows) {
    [Console]::Error.WriteLine('This SDK audit requires Windows and PowerShell 7.2+.')
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
    Write-Host "SDK audit batch: $run"
    [string]$suffix = [guid]::NewGuid().ToString('N')
    [string]$out = Join-Path $root "build\windows-sdk-audit-$suffix"
    New-Item -ItemType Directory -Path $out | Out-Null
    $states = [Collections.Generic.List[object]]::new()

    function Get-LibrarySearchState {
        param([string]$Stage)
        [string[]]$directories = @(
            ([string]$env:LIB -split ';') |
            Where-Object { $_ } |
            ForEach-Object { $_.Trim().Trim('"') }
        )
        $libraries = @(
            'kernel32.lib', 'user32.lib', 'gdi32.lib',
            'shell32.lib', 'pdh.lib', 'comdlg32.lib'
        )
        $found = [ordered]@{}
        foreach ($library in $libraries) {
            $found[$library] = @($directories | Where-Object {
                $candidate = Join-Path $_ $library
                Test-Path -LiteralPath $candidate -PathType Leaf
            })
        }
        $cl = Get-Command cl.exe -CommandType Application -EA Ignore
        [ordered]@{
            stage = $Stage
            developer_arch = $env:VSCMD_ARG_TGT_ARCH
            sdk_version = $env:WindowsSDKVersion
            sdk_root = $env:WindowsSdkDir
            compiler_available = [bool]$cl
            lib_directories = $directories
            resolved_import_directories = $found
        }
    }

    $scriptCode = 0
    Push-Location -LiteralPath $root
    try {
        $states.Add((Get-LibrarySearchState 'initial'))
        Enter-NgenDeveloperShell
        $states.Add((Get-LibrarySearchState 'developer-shell'))

        [string]$debug = Join-Path $run 'Debug'
        $directory = Get-Item -LiteralPath $debug -Force
        if ($directory -isnot [IO.DirectoryInfo] -or
            ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
            throw 'SDK audit requires a regular Debug build directory.'
        }
        foreach ($name in @('conanrun.ps1', 'deactivate_conanrun.ps1')) {
            $item = Get-Item -LiteralPath (Join-Path $debug $name) -Force
            if ($item -isnot [IO.FileInfo] -or
                ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
                $item.Length -gt 2097152) {
                throw 'SDK runtime script is linked, nonregular or oversized.'
            }
        }
        function Invoke-RuntimeRoundTrip {
            try {
                . (Join-Path $debug 'conanrun.ps1')
                $state = Get-LibrarySearchState 'runtime-active'
                $states.Add($state)
            } finally {
                . (Join-Path $debug 'deactivate_conanrun.ps1')
                $state = Get-LibrarySearchState 'runtime-deactivated'
                $states.Add($state)
            }
        }
        Invoke-RuntimeRoundTrip

        [string]$cmake = Join-Path $root (
            'build\windows-tools\Scripts\cmake.exe'
        )
        [string]$release = Join-Path $run 'Release'
        $object = Join-Path $release (
            'CMakeFiles\sim_harness.dir\apps\sim_harness\main.cpp.obj'
        )
        if (-not (Test-Path -LiteralPath $object -PathType Leaf)) {
            throw 'Expected compiled sim_harness object is missing.'
        }
        # Force one relink even if the partial attempt already built it.
        [string[]]$touchArguments = @('-E', 'touch_nocreate', $object)
        & $cmake @touchArguments
        if ($LASTEXITCODE -ne 0) {
            throw 'Could not mark the existing object for relinking.'
        }
        [string[]]$arguments = @(
            '--build', $release,
            '--target', 'sim_harness', '-j', '2'
        )
        $stdout = Join-Path $out 'link.txt'
        $stderr = Join-Path $out 'link.stderr.txt'
        & $cmake @arguments 1> $stdout 2> $stderr
        [int]$linkExit = $LASTEXITCODE
        $scriptCode = $linkExit
        [ordered]@{
            source = (git rev-parse HEAD).Trim()
            target = 'sim_harness'
            exit_code = $linkExit
        } | ConvertTo-Json |
            Set-Content -LiteralPath (Join-Path $out 'link-result.json')
    } catch {
        $scriptCode = 1
        $_ | Out-String |
            Set-Content -LiteralPath (Join-Path $out 'failure.txt')
    } finally {
        try {
            $states | ConvertTo-Json -Depth 8 |
                Set-Content -LiteralPath (Join-Path $out 'sdk-search.json')
            [string]$archive = $out + '.zip'
            Compress-Archive -Path (Join-Path $out '*') -DestinationPath $archive
            Write-Host "SEND SDK AUDIT: $archive"
        } finally { Pop-Location }
    }
    exit $scriptCode
}
