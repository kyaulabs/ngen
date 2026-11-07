#Requires -Version 7.2
[CmdletBinding(DefaultParameterSetName = 'CPU')]
param(
    [Parameter(ParameterSetName = 'CPU')]
    [ValidateSet('Debug', 'Release', 'All')]
    [string]$Configuration = 'All',
    [Parameter(ParameterSetName = 'CPU')]
    [ValidateSet('MSVC', 'ClangCl')]
    [string]$Compiler = 'MSVC',
    [Parameter(ParameterSetName = 'CPU')]
    [switch]$Renderer,
    [Parameter(Mandatory, ParameterSetName = 'GPU')]
    [switch]$GpuInfo
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
    [IO.File]::WriteAllText($LogPath, '')
    & $FilePath @Arguments 2>&1 | Tee-Object -FilePath $LogPath | Out-Host
    $code = $LASTEXITCODE
    if ($code -ne 0) {
        throw "$FilePath failed with exit code $code. See $LogPath"
    }
}

function Get-NgenVulkanSdkRoot {
    param([string]$ProcessRoot, [string]$UserRoot, [string]$MachineRoot)
    foreach ($sdk in @($ProcessRoot, $UserRoot, $MachineRoot)) {
        if ($sdk) { return $sdk }
    }
    return ''
}

function Invoke-NgenRenderer {
    param(
        [Parameter(Mandatory)][string]$BuildDirectory,
        [Parameter(Mandatory)][string]$LogDirectory,
        [Parameter(Mandatory)][string]$Configuration,
        [Parameter(Mandatory)][scriptblock]$Checks
    )
    # SDL is a locked Conan dependency, not the SDK's optional SDL bundle.
    # CTest's startup executable needs the same DLL path as the GPU probes.
    try {
        . (Join-Path $BuildDirectory 'conanrun.ps1')
        & $Checks
        $probe = Join-Path $BuildDirectory 'render_probe.exe'
        Invoke-NgenNative -FilePath $probe -Arguments @('--check-validation-callback') -LogPath (Join-Path $LogDirectory "$Configuration-validation-callback.txt")
        Invoke-NgenNative -FilePath $probe -LogPath (Join-Path $LogDirectory "$Configuration-render-startup.txt")
        Invoke-NgenNative -FilePath $probe -Arguments @('--check-presentation') -LogPath (Join-Path $LogDirectory "$Configuration-presentation.txt")
        Invoke-NgenNative -FilePath (Join-Path $BuildDirectory 'resource_probe.exe') -LogPath (Join-Path $LogDirectory "$Configuration-resources.txt")
        Invoke-NgenNative -FilePath (Join-Path $BuildDirectory 'shader_gpu_probe.exe') -LogPath (Join-Path $LogDirectory "$Configuration-shaders.txt")
        $manifests = @(Get-ChildItem -LiteralPath (Join-Path $BuildDirectory 'shaders') -Filter 'manifest.json' -Recurse -File)
        if ($manifests.Count -ne 1) { throw 'Expected one immutable shader package in the fresh build.' }
        Copy-Item -LiteralPath $manifests[0].FullName -Destination (Join-Path $LogDirectory "$Configuration-shader-manifest.json")
        foreach ($shader in @('cull', 'pbr-vertex', 'pbr-fragment', 'post-vertex', 'post-fragment')) {
            Copy-Item -LiteralPath (Join-Path $manifests[0].DirectoryName "$shader.reflection.json") -Destination (Join-Path $LogDirectory "$Configuration-$shader.reflection.json")
        }
    } finally {
        . (Join-Path $BuildDirectory 'deactivate_conanrun.ps1')
    }
}

function Get-NgenClangProfile {
    param(
        [Parameter(Mandatory)][string]$CompilerPath,
        [Parameter(Mandatory)][string]$VersionText,
        [Parameter(Mandatory)][string]$ToolVersion
    )
    if ($VersionText -notmatch '(?m)^clang version (\d+)\.') {
        throw 'Could not determine the clang-cl version.'
    }
    $major = $Matches[1]
    if ($VersionText -notmatch '(?m)^Target:\s*x86_64-[\w-]*windows-msvc\b') {
        throw 'clang-cl must target x86_64 Windows with the MSVC ABI.'
    }
    $version = $null
    if (-not [version]::TryParse($ToolVersion, [ref]$version) -or $version.Major -ne 14) {
        throw 'Could not determine a supported MSVC runtime from VCToolsVersion.'
    }
    # Conan maps v143 to MSVC 14.3 and v144 to MSVC 14.4. Do not use
    # Conan detection's assumed v143 when the active toolset is 14.4.
    $runtime = "v14$([int][math]::Floor($version.Minor / 10))"
    $executables = @{ c = $CompilerPath; cpp = $CompilerPath } | ConvertTo-Json -Compress
    return @"
[settings]
os=Windows
arch=x86_64
compiler=clang
compiler.version=$major
compiler.runtime=dynamic
compiler.runtime_version=$runtime
compiler.cppstd=20
build_type=Release
[conf]
tools.cmake.cmaketoolchain:generator=Ninja
tools.build:compiler_executables=$executables
"@
}

function Invoke-NgenGpuInfo {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter(Mandatory)][string]$LogDirectory
    )
    $LogDirectory = (Resolve-Path -LiteralPath $LogDirectory).Path
    Invoke-NgenNative -FilePath $FilePath -Arguments @('--summary') -LogPath (Join-Path $LogDirectory 'vulkan-summary.txt')
    Push-Location $LogDirectory
    try {
        Invoke-NgenNative -FilePath $FilePath -Arguments @('--json') -LogPath (Join-Path $LogDirectory 'vulkan-json.txt')
    } finally {
        Pop-Location
    }
    $profiles = @(Get-ChildItem -LiteralPath $LogDirectory -Filter 'VP_*.json' -File)
    if (-not $profiles.Count) { throw 'vulkaninfo completed without producing a GPU JSON profile.' }
    foreach ($profile in $profiles) {
        $data = Get-Content -LiteralPath $profile.FullName -Raw | ConvertFrom-Json -AsHashtable
        if ($data -isnot [System.Collections.IDictionary] -or -not $data.Contains('capabilities')) {
            throw "Missing Vulkan capabilities in $($profile.Name)"
        }
    }
    Write-Host 'GPU capability capture completed. This is not a rendering or presentation test.'
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
    Write-Host "Run directory: $run"
    Invoke-NgenNative -FilePath 'git' -Arguments @('rev-parse', 'HEAD') -LogPath (Join-Path $logs 'revision.txt')
    Invoke-NgenNative -FilePath 'git' -Arguments @('status', '--short') -LogPath (Join-Path $logs 'working-tree.txt')
    if ($GpuInfo) {
        $vulkan = Get-Command vulkaninfo.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
        $vulkanPath = if ($vulkan) { $vulkan.Source } else { $null }
        if (-not $vulkanPath -and $env:VULKAN_SDK) {
            $candidate = Join-Path $env:VULKAN_SDK 'Bin/vulkaninfo.exe'
            if (Test-Path -LiteralPath $candidate) { $vulkanPath = $candidate }
        }
        if (-not $vulkanPath) {
            throw 'vulkaninfo was not found. Install the Windows Vulkan SDK with Vulkan Tools from https://vulkan.lunarg.com/sdk/home#windows, open a new PowerShell window and rerun with -GpuInfo. No SDK or driver is installed by this script.'
        }
        (Get-Item $vulkanPath).VersionInfo | Format-List FileName, FileVersion, ProductVersion | Out-Host
        Get-FileHash -LiteralPath $vulkanPath -Algorithm SHA256 | Format-List | Out-File (Join-Path $logs 'vulkan-tool.txt')
        Invoke-NgenGpuInfo -FilePath $vulkanPath -LogDirectory $logs
        # exit still runs the shared finally block that packages the evidence.
        exit 0
    }

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
    $msvc = Get-Command cl -CommandType Application -ErrorAction Stop | Select-Object -First 1
    (Get-Item $msvc.Source).VersionInfo | Format-List FileName, FileVersion, ProductVersion | Out-Host
    $clangPath = $null
    if ($Compiler -eq 'ClangCl') {
        $command = Get-Command clang-cl.exe -CommandType Application -ErrorAction SilentlyContinue | Select-Object -First 1
        if ($command) { $clangPath = $command.Source }
        foreach ($candidate in @(
            "$env:VSINSTALLDIR/VC/Tools/Llvm/x64/bin/clang-cl.exe",
            "$env:ProgramFiles/LLVM/bin/clang-cl.exe"
        )) {
            if (-not $clangPath -and (Test-Path -LiteralPath $candidate)) { $clangPath = $candidate }
        }
        if (-not $clangPath) {
            throw 'clang-cl was not found. In Visual Studio Installer, Modify > Desktop development with C++ > C++ Clang tools for Windows, then rerun with -Compiler ClangCl.'
        }
        Invoke-NgenNative -FilePath $clangPath -Arguments @('--version') -LogPath (Join-Path $logs 'clang-cl-version.txt')
    }
    Write-Host "Selected compiler: $Compiler"
    Write-Host "VCToolsVersion: $env:VCToolsVersion"
    Write-Host "WindowsSDKVersion: $env:WindowsSDKVersion"
    if ($Renderer) {
        # Existing terminals may predate SDK installation. Preserve an explicit
        # process setting; otherwise use the configured user/machine SDK.
        $env:VULKAN_SDK = Get-NgenVulkanSdkRoot -ProcessRoot $env:VULKAN_SDK `
            -UserRoot ([Environment]::GetEnvironmentVariable('VULKAN_SDK', 'User')) `
            -MachineRoot ([Environment]::GetEnvironmentVariable('VULKAN_SDK', 'Machine'))
        "VULKAN_SDK=$env:VULKAN_SDK" | Tee-Object -FilePath (Join-Path $logs 'vulkan-sdk.txt') | Out-Host
    }
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
    $buildProfile = Join-Path $run 'msvc-profile'
    $oldCC, $oldCXX = $env:CC, $env:CXX
    try {
        $env:CC = $msvc.Source
        $env:CXX = $msvc.Source
        Invoke-NgenNative -FilePath $conan -Arguments @('profile', 'detect', '--name', $buildProfile) -LogPath (Join-Path $logs 'profile-detect.txt')
    } finally {
        $env:CC = $oldCC
        $env:CXX = $oldCXX
    }
    $profile = $buildProfile
    if ($Compiler -eq 'ClangCl') {
        $profile = Join-Path $run 'clang-cl-profile'
        Get-NgenClangProfile -CompilerPath $clangPath -VersionText (Get-Content -Raw (Join-Path $logs 'clang-cl-version.txt')) -ToolVersion $env:VCToolsVersion |
            Set-Content -LiteralPath $profile
    }
    Copy-Item -LiteralPath $profile -Destination (Join-Path $logs 'host-profile.txt')
    Copy-Item -LiteralPath $buildProfile -Destination (Join-Path $logs 'build-profile.txt')
    Invoke-NgenNative -FilePath $conan -Arguments @('profile', 'show', '-pr:h', $profile, '-pr:b', $buildProfile) -LogPath (Join-Path $logs 'profile.txt')
    $configurations = switch ($Configuration) {
        'All' { 'Debug'; 'Release' }
        'Debug' { 'Debug' }
        'Release' { 'Release' }
    }
    if ($Renderer) {
        Invoke-NgenNative -FilePath $conan -Arguments @('export', 'recipes/slangc', '--version=2026.21', '--user=ngen', '--channel=tools') -LogPath (Join-Path $logs 'slang-recipe.txt')
    }
    $lockfile = if ($Renderer) { 'conan-render.lock' } else { 'conan.lock' }
    $rendererOption = if ($Renderer) { 'True' } else { 'False' }
    $rendererBuild = if ($Renderer) { 'ON' } else { 'OFF' }
    foreach ($config in $configurations) {
        $build = Join-Path $run $config
        $buildType = if ($Renderer) { 'Release' } else { $config }
        Invoke-NgenNative -FilePath $conan -Arguments @(
            'install', '.', '-pr:h', $profile, '-pr:b', $buildProfile,
            '-s:h', 'compiler.cppstd=20', '-s:b', 'compiler.cppstd=20',
            '-s:h', "build_type=$config", '-s:b', "build_type=$buildType",
            '-c:h', 'tools.cmake.cmaketoolchain:generator=Ninja',
            '-c:b', 'tools.cmake.cmaketoolchain:generator=Ninja',
            '-c:h', 'tools.build:jobs=2', '-c:b', 'tools.build:jobs=2',
            '-o:h', "&:renderer=$rendererOption", '-o:h', "&:shaders=$rendererOption",
            '--lockfile', $lockfile, '--build=missing', '-of', $build
        ) -LogPath (Join-Path $logs "$config-conan.txt")
        $toolchain = Join-Path $build 'conan_toolchain.cmake'
        Invoke-NgenNative -FilePath $cmake -Arguments @(
            '-S', $root, '-B', $build, '-G', 'Ninja',
            "-DCMAKE_TOOLCHAIN_FILE=$toolchain", "-DCMAKE_BUILD_TYPE=$config",
            "-DNGEN_BUILD_RENDERER=$rendererBuild"
        ) -LogPath (Join-Path $logs "$config-configure.txt")
        Invoke-NgenNative -FilePath $cmake -Arguments @('--build', $build, '-j', '2') -LogPath (Join-Path $logs "$config-build.txt")
        $checks = {
            Invoke-NgenNative -FilePath $ctest -Arguments @('--test-dir', $build, '--show-only') -LogPath (Join-Path $logs "$config-test-list.txt")
            Invoke-NgenNative -FilePath $ctest -Arguments @('--test-dir', $build, '--output-on-failure', '--no-tests=error') -LogPath (Join-Path $logs "$config-tests.txt")
            $harness = Join-Path $build 'sim_harness.exe'
            Invoke-NgenNative -FilePath $harness -LogPath (Join-Path $logs "$config-serial.txt")
            Invoke-NgenNative -FilePath $harness -Arguments @('--workers=2') -LogPath (Join-Path $logs "$config-workers.txt")
        }
        if ($Renderer) {
            Invoke-NgenRenderer -BuildDirectory $build -LogDirectory $logs -Configuration $config -Checks $checks
        } else {
            & $checks
        }
    }
    if ($Renderer) {
        Write-Host "Windows $Compiler CPU tests, Vulkan startup/presentation, GPU resource retirement and native shader culling/PBR fixture checks passed. This is not the imported-ship rendering path or full M2 acceptance."
    } else {
        Write-Host "Windows $Compiler CPU verification passed. This is not a Vulkan rendering test."
    }
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
