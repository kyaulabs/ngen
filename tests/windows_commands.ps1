#Requires -Version 7.2
$ErrorActionPreference = 'Stop'
$PSNativeCommandUseErrorActionPreference = $false
$root = Split-Path -Parent $PSScriptRoot
$builder = Join-Path $root 'tools/build-windows.ps1'
$pwsh = (Get-Process -Id $PID).Path
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('ngen commands ' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $temporary | Out-Null
try {
    $log = Join-Path $temporary 'import.txt'
    $command = ". '$builder'; if (-not (Get-Command Invoke-NgenFlight -ErrorAction Ignore)) { exit 8 }; Write-Output 'functions-only-import'"
    & $pwsh -NoLogo -NoProfile -Command $command *> $log
    if ($LASTEXITCODE -ne 0 -or (Get-Content -Raw -LiteralPath $log).Trim() -cne 'functions-only-import') {
        throw 'Importing build functions bootstrapped/exited instead of returning to the caller.'
    }
    . $builder
    $fixtureRoot = Join-Path $temporary 'project with spaces'
    $runs = Join-Path $fixtureRoot 'build/windows'
    function New-FlightRun {
        param([string]$Name, [string]$Compiler)
        $path = Join-Path $runs $Name
        New-Item -ItemType Directory -Path (Join-Path $path 'logs'), (Join-Path $path 'Release') | Out-Null
        Set-Content -LiteralPath (Join-Path $path 'logs/host-profile.txt') -Value "[settings]`nos=Windows`narch=x86_64`ncompiler=$Compiler`ncompiler.runtime=dynamic"
        Set-Content -LiteralPath (Join-Path $path 'Release/CMakeCache.txt') -Value "CMAKE_BUILD_TYPE:STRING=Release`nCMAKE_HOME_DIRECTORY:INTERNAL=$fixtureRoot`nNGEN_BUILD_FLIGHT_DEMO:BOOL=ON"
        Set-Content -LiteralPath (Join-Path $path 'Release/demo-runtime.json') -Value '{"schema":1,"os":"Windows","configuration":"Release","sanitizer":""}'
        foreach ($name in @('build-profile.txt', 'clang-cl-version.txt', 'Release-configure.txt')) {
            Set-Content -LiteralPath (Join-Path $path "logs/$name") -Value 'fixture metadata'
        }
        foreach ($name in @('conanrun.ps1', 'deactivate_conanrun.ps1')) {
            Set-Content -LiteralPath (Join-Path $path "Release/$name") -Value '# fixture'
        }
        return $path
    }
    $oldClang = New-FlightRun '20261114-120000-000' 'clang'
    $newClang = New-FlightRun '20261115-120000-000' 'clang'
    $msvc = New-FlightRun '20261115-130000-000' 'msvc'
    $selected = Get-NgenWindowsFlightRun -SourceRoot $fixtureRoot -Compiler ClangCl
    if ($selected -cne $newClang) { throw 'Run discovery did not select the latest matching ClangCl flight batch.' }
    $selected = Get-NgenWindowsFlightRun -SourceRoot $fixtureRoot -Compiler MSVC
    if ($selected -cne $msvc) { throw 'Run discovery confused MSVC and ClangCl batches.' }
    $bundle = Join-Path $msvc 'offline/ngen-flight'
    New-Item -ItemType Directory -Path $bundle | Out-Null
    Set-Content -LiteralPath (Join-Path $bundle 'manifest.json') -Value '{"schema":1}'
    $unfinished = New-FlightRun '20261115-140000-000' 'msvc'
    $selected = Get-NgenWindowsFlightRun -SourceRoot $fixtureRoot -Compiler MSVC -RequireBundle
    if ($selected -cne $msvc) { throw 'Timing discovery selected an unfinished batch instead of a delivered bundle.' }
    $foreign = New-FlightRun '20261115-150000-000' 'clang'
    Set-Content -LiteralPath (Join-Path $foreign 'Release/CMakeCache.txt') -Value "CMAKE_BUILD_TYPE:STRING=Release`nCMAKE_HOME_DIRECTORY:INTERNAL=$temporary`nNGEN_BUILD_FLIGHT_DEMO:BOOL=ON"
    $selected = Get-NgenWindowsFlightRun -SourceRoot $fixtureRoot -Compiler ClangCl
    if ($selected -cne $newClang) { throw 'Run discovery selected a cache for another source project.' }
    $wrongAbi = New-FlightRun '20261115-170000-000' 'clang'
    Set-Content -LiteralPath (Join-Path $wrongAbi 'logs/host-profile.txt') -Value "[settings]`nos=Windows`narch=x86_64`ncompiler=clang"
    $selected = Get-NgenWindowsFlightRun -SourceRoot $fixtureRoot -Compiler ClangCl
    if ($selected -cne $newClang) { throw 'Discovery selected a different Windows compiler ABI/runtime profile.' }
    $selected = Get-NgenWindowsFlightRun -SourceRoot $fixtureRoot -Compiler ClangCl -RunDirectory $oldClang
    if ($selected -cne $oldClang) { throw 'An explicit matching batch was not respected.' }
    $rejected = $false
    try { Get-NgenWindowsFlightRun -SourceRoot $fixtureRoot -Compiler MSVC -RunDirectory $newClang | Out-Null } catch { $rejected = $true }
    if (-not $rejected) { throw 'An explicit wrong-compiler batch was accepted.' }
    $outside = Join-Path $temporary 'outside'
    New-Item -ItemType Directory -Path $outside | Out-Null
    $rejected = $false
    try { Get-NgenWindowsFlightRun -SourceRoot $fixtureRoot -Compiler ClangCl -RunDirectory $outside | Out-Null } catch { $rejected = $true }
    if (-not $rejected) { throw 'A batch outside the declared project was accepted.' }
    if (-not $IsWindows) {
        $linked = New-FlightRun '20261115-160000-000' 'clang'
        $profilePath = Join-Path $linked 'logs/host-profile.txt'
        Remove-Item -LiteralPath $profilePath
        New-Item -ItemType SymbolicLink -Path $profilePath -Target (Join-Path $newClang 'logs/host-profile.txt') | Out-Null
        $selected = Get-NgenWindowsFlightRun -SourceRoot $fixtureRoot -Compiler ClangCl
        if ($selected -cne $newClang) { throw 'Discovery followed a linked profile.' }
    }
    if (-not $IsWindows) {
        $linkedMetadata = New-FlightRun '20261115-180000-000' 'clang'
        $linkedPath = Join-Path $linkedMetadata 'logs/build-profile.txt'
        Remove-Item -LiteralPath $linkedPath
        New-Item -ItemType SymbolicLink -Path $linkedPath -Target (Join-Path $newClang 'logs/build-profile.txt') | Out-Null
        $selected = Get-NgenWindowsFlightRun -SourceRoot $fixtureRoot -Compiler ClangCl
        if ($selected -cne $newClang) { throw 'Discovery admitted linked metadata that would be copied into evidence.' }
    }
    $audioOnly = New-FlightRun '20261115-190000-000' 'clang'
    Set-Content -LiteralPath (Join-Path $audioOnly 'Release/CMakeCache.txt') -Value "CMAKE_BUILD_TYPE:STRING=Release`nCMAKE_HOME_DIRECTORY:INTERNAL=$fixtureRoot`nNGEN_BUILD_FLIGHT_DEMO:BOOL=OFF"
    $selected = Get-NgenWindowsFlightRun -SourceRoot $fixtureRoot -Compiler ClangCl
    if ($selected -cne $newClang) { throw 'Discovery selected an audio-only cache without the flight target.' }
    $invalidRuntime = New-FlightRun '20261115-200000-000' 'clang'
    foreach ($json in @('[]', '{"schema":1,"os":"Linux","configuration":"Release"}', '{"schema":1,"os":"Windows","configuration":"Debug"}', '{"schema":1,"os":"Windows","configuration":"Release","sanitizer":"address"}', '{"schema":')) {
        Set-Content -LiteralPath (Join-Path $invalidRuntime 'Release/demo-runtime.json') -Value $json
        $selected = Get-NgenWindowsFlightRun -SourceRoot $fixtureRoot -Compiler ClangCl
        if ($selected -cne $newClang) { throw 'Discovery selected malformed or non-deliverable runtime metadata.' }
    }
    if (-not $IsWindows) {
        $programFiles = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)', 'Process')
        $fakeVs = $env:NGEN_FAKE_VS
        $sdkWorking = Join-Path $temporary 'empty SDK working directory'
        New-Item -ItemType Directory -Path $sdkWorking | Out-Null
        Push-Location -LiteralPath $sdkWorking
        try {
            $programRoot = Join-Path $temporary 'Program Files'
            $installer = Join-Path $programRoot 'Microsoft Visual Studio/Installer'
            New-Item -ItemType Directory -Path $installer | Out-Null
            $query = Join-Path $installer 'vswhere.exe'
            Set-Content -LiteralPath $query -Value @'
#!/bin/sh
[ "$#" -eq 7 ] || exit 7
[ "$1" = -latest ] && [ "$2" = -products ] && [ "$3" = '*' ] || exit 7
[ "$4" = -requires ] || exit 7
[ "$5" = Microsoft.VisualStudio.Component.VC.Tools.x86.x64 ] || exit 7
[ "$6" = -property ] && [ "$7" = installationPath ] || exit 7
printf '%s\n' "$NGEN_FAKE_VS"
'@
            & chmod +x $query
            if ($LASTEXITCODE -ne 0) { throw 'Could not prepare the SDK query boundary.' }
            [Environment]::SetEnvironmentVariable('ProgramFiles(x86)', $programRoot, 'Process')
            $env:NGEN_FAKE_VS = Join-Path $temporary 'Visual Studio with spaces'
            $script:sdkImported = $false; $script:sdkEntered = $false
            function Import-Module {
                param([string]$Name)
                if ($Name -cne (Join-Path $env:NGEN_FAKE_VS 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')) { throw 'Wrong SDK module path.' }
                $script:sdkImported = $true
            }
            function Enter-VsDevShell {
                param([string]$VsInstallPath, [switch]$SkipAutomaticLocation, [string]$DevCmdArguments)
                if ($VsInstallPath -cne $env:NGEN_FAKE_VS -or -not $SkipAutomaticLocation -or $DevCmdArguments -cne '-arch=x64 -host_arch=x64') { throw 'Wrong developer-shell arguments.' }
                $script:sdkEntered = $true
            }
            Enter-NgenDeveloperShell
            if (-not $script:sdkImported -or -not $script:sdkEntered) { throw 'Developer-shell initialization did not reach its system boundaries.' }
            Set-Content -LiteralPath $query -Value "#!/bin/sh`nexit 7"
            $script:sdkImported = $false; $rejected = $false
            try { Enter-NgenDeveloperShell } catch { $rejected = $true }
            if (-not $rejected -or $script:sdkImported) { throw 'Failed SDK discovery was hidden or still imported a module.' }
        } finally {
            [Environment]::SetEnvironmentVariable('ProgramFiles(x86)', $(if ($null -eq $programFiles) { [NullString]::Value } else { $programFiles }), 'Process')
            $env:NGEN_FAKE_VS = $fakeVs
            Pop-Location
            Remove-Item -LiteralPath Function:Import-Module, Function:Enter-VsDevShell -ErrorAction Ignore
        }
    }
    foreach ($name in @('windows-sdk-audit.ps1', 'resume-windows-flight.ps1', 'repeat-windows-flight-timing.ps1')) {
        $path = Join-Path $root "tools/$name"
        $tokens = $null; $errors = $null
        [void][Management.Automation.Language.Parser]::ParseFile($path, [ref]$tokens, [ref]$errors)
        if ($errors.Count) { throw ($errors | Out-String) }
        if (-not $IsWindows) {
            & $pwsh -NoLogo -NoProfile -File $path *> $log
            if ($LASTEXITCODE -ne 2 -or (Get-Content -Raw -LiteralPath $log) -notmatch 'requires? Windows') {
                throw "The $name entry point did not reject its unsupported host before native work."
            }
        }
    }
    Write-Host 'PASS: Windows command entry boundaries (not native Windows execution)'
} finally {
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
exit 0
