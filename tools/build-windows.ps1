#Requires -Version 7.2
[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release', 'All')]
    [string]$Configuration = 'All'
)

function Invoke-NgenNative {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [string[]]$Arguments = @(),
        [Parameter(Mandatory)][string]$LogPath
    )
    # Capture both streams explicitly; transcript alone can omit native output.
    # We check every exit code ourselves, regardless of the caller's PS setting.
    $PSNativeCommandUseErrorActionPreference = $false
    & $FilePath @Arguments 2>&1 | Tee-Object -FilePath $LogPath | Out-Host
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        throw "$FilePath failed with exit code $code. See $LogPath"
    }
}

if (-not $IsWindows) {
    [Console]::Error.WriteLine('This build script requires Windows and PowerShell 7.2 or newer.')
    exit 2
}

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$run = Join-Path $root ('build/windows/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff'))
$logs = Join-Path $run 'logs'
$tools = Join-Path $root 'build/windows-tools'
New-Item -ItemType Directory -Path $logs -Force | Out-Null
$exitCode = 1
$transcribing = $false
Push-Location $root
try {
    Start-Transcript -Path (Join-Path $logs 'session.txt') | Out-Host
    $transcribing = $true
    Write-Host "Build directory: $run"

    if (-not (Get-Command cl -ErrorAction SilentlyContinue) -or $env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) {
            throw 'Install Visual Studio Build Tools with Desktop development with C++, then rerun.'
        }
        $installation = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if ($LASTEXITCODE -ne 0 -or -not $installation) {
            throw 'MSVC x64/x86 tools were not found. Add Desktop development with C++ in Visual Studio Installer.'
        }
        Import-Module (Join-Path $installation 'Common7/Tools/Microsoft.VisualStudio.DevShell.dll')
        Enter-VsDevShell -VsInstallPath $installation -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64'
    }
    $compiler = Get-Command cl -ErrorAction Stop
    (Get-Item $compiler.Source).VersionInfo | Format-List FileName, FileVersion, ProductVersion | Out-Host
    Write-Host "VCToolsVersion: $env:VCToolsVersion"
    Write-Host "WindowsSDKVersion: $env:WindowsSDKVersion"
    Invoke-NgenNative -FilePath 'git' -Arguments @('rev-parse', 'HEAD') -LogPath (Join-Path $logs 'revision.txt')
    Invoke-NgenNative -FilePath 'git' -Arguments @('status', '--short') -LogPath (Join-Path $logs 'working-tree.txt')

    $python = Join-Path $tools 'Scripts/python.exe'
    if (-not (Test-Path -LiteralPath $python)) {
        if (Get-Command py -ErrorAction SilentlyContinue) {
            Invoke-NgenNative -FilePath 'py' -Arguments @('-3', '-m', 'venv', $tools) -LogPath (Join-Path $logs 'venv.txt')
        } elseif (Get-Command python -ErrorAction SilentlyContinue) {
            Invoke-NgenNative -FilePath 'python' -Arguments @('-m', 'venv', $tools) -LogPath (Join-Path $logs 'venv.txt')
        } else {
            throw 'Python 3 is required. Install it with pip support and rerun.'
        }
    }
    Invoke-NgenNative -FilePath $python -Arguments @('-m', 'pip', 'install', 'conan==2.34.0', 'cmake==4.4.4', 'ninja==1.13.2') -LogPath (Join-Path $logs 'tools-install.txt')
    $env:Path = (Join-Path $tools 'Scripts') + [IO.Path]::PathSeparator + $env:Path
    $conan = Join-Path $tools 'Scripts/conan.exe'
    $cmake = Join-Path $tools 'Scripts/cmake.exe'
    $ctest = Join-Path $tools 'Scripts/ctest.exe'
    Invoke-NgenNative -FilePath $conan -Arguments @('--version') -LogPath (Join-Path $logs 'conan-version.txt')
    Invoke-NgenNative -FilePath $cmake -Arguments @('--version') -LogPath (Join-Path $logs 'cmake-version.txt')
    Invoke-NgenNative -FilePath 'ninja' -Arguments @('--version') -LogPath (Join-Path $logs 'ninja-version.txt')

    # Detect into this run's generated directory, never overwrite a user profile.
    $profile = Join-Path $run 'profile'
    Invoke-NgenNative -FilePath $conan -Arguments @('profile', 'detect', '--name', $profile) -LogPath (Join-Path $logs 'profile-detect.txt')
    Invoke-NgenNative -FilePath $conan -Arguments @('profile', 'show', '-pr:h', $profile, '-pr:b', $profile) -LogPath (Join-Path $logs 'profile.txt')
    $configurations = switch ($Configuration) {
        'All' { 'Debug'; 'Release' }
        'Debug' { 'Debug' }
        'Release' { 'Release' }
    }
    foreach ($config in $configurations) {
        $build = Join-Path $run $config
        Invoke-NgenNative -FilePath $conan -Arguments @(
            'install', '.', '-pr:h', $profile, '-pr:b', $profile,
            '-s:h', 'compiler.cppstd=20', '-s:b', 'compiler.cppstd=20',
            '-s:h', "build_type=$config", '-s:b', "build_type=$config",
            '-c:h', 'tools.cmake.cmaketoolchain:generator=Ninja',
            '-c:b', 'tools.cmake.cmaketoolchain:generator=Ninja',
            '--lockfile', 'conan.lock', '--build=missing', '-of', $build
        ) -LogPath (Join-Path $logs "$config-conan.txt")
        $toolchain = Join-Path $build 'conan_toolchain.cmake'
        Invoke-NgenNative -FilePath $cmake -Arguments @(
            '-S', $root, '-B', $build, '-G', 'Ninja',
            "-DCMAKE_TOOLCHAIN_FILE=$toolchain", "-DCMAKE_BUILD_TYPE=$config",
            '-DNGEN_BUILD_RENDERER=OFF'
        ) -LogPath (Join-Path $logs "$config-configure.txt")
        Invoke-NgenNative -FilePath $cmake -Arguments @('--build', $build, '-j', '2') -LogPath (Join-Path $logs "$config-build.txt")
        Invoke-NgenNative -FilePath $ctest -Arguments @('--test-dir', $build, '--show-only') -LogPath (Join-Path $logs "$config-test-list.txt")
        Invoke-NgenNative -FilePath $ctest -Arguments @('--test-dir', $build, '--output-on-failure', '--no-tests=error') -LogPath (Join-Path $logs "$config-tests.txt")
        $harness = Join-Path $build 'sim_harness.exe'
        Invoke-NgenNative -FilePath $harness -LogPath (Join-Path $logs "$config-serial.txt")
        Invoke-NgenNative -FilePath $harness -Arguments @('--workers=2') -LogPath (Join-Path $logs "$config-workers.txt")
    }
    Write-Host 'Windows CPU verification passed. This is not a Vulkan rendering test.'
    $exitCode = 0
} catch {
    $_ | Out-String | Tee-Object -FilePath (Join-Path $logs 'failure.txt') | Out-Host
} finally {
    if ($transcribing) { Stop-Transcript | Out-Host }
    try {
        Compress-Archive -Path (Join-Path $logs '*') -DestinationPath (Join-Path $run 'evidence.zip')
        Write-Host "Send this archive: $(Join-Path $run 'evidence.zip')"
    } catch {
        Write-Warning "Could not zip evidence. Send the files in $logs"
    }
    Pop-Location
}
exit $exitCode
