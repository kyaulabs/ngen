#Requires -Version 7.2
$ErrorActionPreference = 'Stop'
$scriptPath = Join-Path $PSScriptRoot '../tools/build-windows.ps1'
$tokens = $null
$parseErrors = $null
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    $scriptPath, [ref]$tokens, [ref]$parseErrors)
if ($parseErrors.Count) { throw ($parseErrors | Out-String) }

# Load only the native-process boundary; never bootstrap a Windows build in
# this test. Exercise real child processes, argument handling and diagnostics.
$definition = $ast.Find({
    param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
    $node.Name -eq 'Invoke-NgenNative'
}, $true)
if (-not $definition) { throw 'Native command boundary not found' }
. ([scriptblock]::Create($definition.Extent.Text))
$pwsh = (Get-Process -Id $PID).Path
$directory = Join-Path ([System.IO.Path]::GetTempPath()) ('ngen script test ' + [guid]::NewGuid())
New-Item -ItemType Directory $directory | Out-Null
try {
    $log = Join-Path $directory 'child.log'
    $failed = $false
    try {
        Invoke-NgenNative -FilePath $pwsh -Arguments @(
            '-NoProfile', '-Command', 'Write-Output "stdout marker"; [Console]::Error.WriteLine("stderr marker"); exit 7'
        ) -LogPath $log
    } catch {
        $failed = $_.Exception.Message -match 'exit code 7'
    }
    if (-not $failed) { throw 'A failed child process was reported as success' }
    $text = Get-Content -Raw $log
    if ($text -notmatch 'stdout marker' -or $text -notmatch 'stderr marker') {
        throw 'Native stdout/stderr was not retained'
    }

    $child = Join-Path $directory 'child with spaces.ps1'
    Set-Content -LiteralPath $child -Value 'param($value) if ($value -cne "one argument with spaces") { exit 9 }; Write-Output $value'
    Invoke-NgenNative -FilePath $pwsh -Arguments @(
        '-NoProfile', '-File', $child, 'one argument with spaces'
    ) -LogPath $log
    if ((Get-Content -Raw $log) -notmatch 'one argument with spaces') {
        throw 'Arguments with spaces were not preserved'
    }

    Invoke-NgenNative -FilePath $pwsh -Arguments @('-NoProfile', '-Command', 'exit 0') -LogPath $log
    if (-not (Test-Path -LiteralPath $log) -or (Get-Item $log).Length -ne 0) {
        throw 'An empty successful command did not produce an empty evidence file'
    }

    & $pwsh -NoProfile -File $scriptPath -Configuration Invalid *> $log
    if ($LASTEXITCODE -eq 0) { throw 'Invalid build configuration was accepted' }
    if (-not $IsWindows) {
        & $pwsh -NoProfile -File $scriptPath -Compiler ClangCl *> $log
        if ($LASTEXITCODE -ne 2 -or (Get-Content -Raw $log) -notmatch 'requires Windows') {
            throw 'Unsupported host did not fail before build preparation'
        }
    }
    if (-not $IsWindows) {
        & $pwsh -NoProfile -File $scriptPath -GpuInfo *> $log
        if ($LASTEXITCODE -ne 2 -or (Get-Content -Raw $log) -notmatch 'requires Windows') {
            throw 'GPU capture mode did not reach the Windows prerequisite check'
        }
    }
    if (-not $IsWindows) {
        & $pwsh -NoProfile -File $scriptPath -Renderer *> $log
        if ($LASTEXITCODE -ne 2 -or (Get-Content -Raw $log) -notmatch 'requires Windows') {
            throw 'Renderer mode did not reach the Windows prerequisite check'
        }
    }
    & $pwsh -NoProfile -File $scriptPath -GpuInfo -Renderer *> $log
    if ($LASTEXITCODE -eq 0 -or $LASTEXITCODE -eq 2) { throw 'GPU capture and renderer builds were mixed' }
    & $pwsh -NoProfile -File $scriptPath -GpuInfo -Compiler ClangCl *> $log
    if ($LASTEXITCODE -eq 0 -or $LASTEXITCODE -eq 2) { throw 'CPU and GPU modes were mixed instead of rejecting ambiguous input' }
    if (-not $IsWindows) {
        $gpuDefinition = $ast.Find({
            param($node)
            $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
            $node.Name -eq 'Invoke-NgenGpuInfo'
        }, $true)
        if (-not $gpuDefinition) { throw 'GPU capture boundary not found' }
        . ([scriptblock]::Create($gpuDefinition.Extent.Text))
        $gpuLogs = Join-Path $directory 'gpu logs'
        New-Item -ItemType Directory $gpuLogs | Out-Null
        $noOutput = Get-Command true -CommandType Application | Select-Object -First 1
        $location = (Get-Location).Path
        $failed = $false
        try {
            Invoke-NgenGpuInfo -FilePath $noOutput.Source -LogDirectory $gpuLogs
        } catch { $failed = $_.Exception.Message -match 'without producing' }
        if (-not $failed) { throw 'GPU capture accepted a successful process that produced no JSON profile' }
        if ((Get-Location).Path -ne $location) { throw 'GPU capture did not restore the working directory' }
        Set-Content -LiteralPath (Join-Path $gpuLogs 'VP_invalid.json') -Value '{"not_capabilities":{}}'
        $failed = $false
        try {
            Invoke-NgenGpuInfo -FilePath $noOutput.Source -LogDirectory $gpuLogs
        } catch { $failed = $_.Exception.Message -match 'Missing Vulkan capabilities' }
        if (-not $failed) { throw 'GPU capture accepted unrelated JSON as a Vulkan profile' }
    }
    if (-not $IsWindows) {
        $rendererDefinition = $ast.Find({
            param($node)
            $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
            $node.Name -eq 'Invoke-NgenRenderer'
        }, $true)
        if (-not $rendererDefinition) { throw 'Renderer process boundary not found' }
        . ([scriptblock]::Create($rendererDefinition.Extent.Text))
        $renderBuild = Join-Path $directory 'renderer build'
        New-Item -ItemType Directory $renderBuild | Out-Null
        Set-Content (Join-Path $renderBuild 'conanrun.ps1') '$env:NGEN_RENDER_ENV = "active"'
        Set-Content (Join-Path $renderBuild 'deactivate_conanrun.ps1') '$env:NGEN_RENDER_ENV = "before"'
        $shaderPackage = Join-Path $renderBuild 'shaders/cooked'
        New-Item -ItemType Directory $shaderPackage | Out-Null
        Set-Content (Join-Path $shaderPackage 'manifest.json') '{"schema":1,"compiler":{"sha256":"fixture compiler identity"}}'
        foreach ($shader in @('cull', 'pbr-vertex', 'pbr-fragment')) {
            Set-Content (Join-Path $shaderPackage "$shader.reflection.json") '{"fixture":"reflected layout"}'
        }
        $probe = Join-Path $renderBuild 'render_probe.exe'
        Set-Content -LiteralPath $probe -Value @'
#!/bin/sh
[ "$NGEN_RENDER_ENV" = active ] || exit 9
mode=${1:-startup}
case "$0" in
  */resource_probe.exe) mode=resources ;;
  */shader_gpu_probe.exe) mode=shaders ;;
esac
printf '%s\n' "$mode"
[ "$mode" != "$NGEN_PROBE_FAIL_AT" ] || exit 7
exit 0
'@
        $resourceProbe = Join-Path $renderBuild 'resource_probe.exe'
        Copy-Item -LiteralPath $probe -Destination $resourceProbe
        $shaderProbe = Join-Path $renderBuild 'shader_gpu_probe.exe'
        Copy-Item -LiteralPath $probe -Destination $shaderProbe
        & chmod +x $probe $resourceProbe $shaderProbe
        if ($LASTEXITCODE -ne 0) { throw 'Could not prepare renderer subprocess fixture' }
        foreach ($failAt in @('', 'checks', '--check-validation-callback', 'startup', '--check-presentation', 'resources', 'shaders')) {
            $renderLogs = Join-Path $directory ('render logs ' + [guid]::NewGuid())
            New-Item -ItemType Directory $renderLogs | Out-Null
            $env:NGEN_RENDER_ENV = 'before'
            $env:NGEN_PROBE_FAIL_AT = $failAt
            $failed = $false
            $checks = {
                Invoke-NgenNative -FilePath $probe -Arguments @('checks') -LogPath (Join-Path $renderLogs 'Debug-checks.txt')
            }
            try {
                Invoke-NgenRenderer -BuildDirectory $renderBuild -LogDirectory $renderLogs -Configuration Debug -Checks $checks
            } catch { $failed = $_.Exception.Message -match 'exit code 7' }
            if ($failed -ne ($failAt -ne '')) { throw 'Renderer probe failure was lost or falsely reported' }
            if ($env:NGEN_RENDER_ENV -ne 'before') { throw 'Renderer runtime environment was not restored' }
            $startupLog = Join-Path $renderLogs 'Debug-render-startup.txt'
            if ($failAt -eq 'checks' -and (Test-Path (Join-Path $renderLogs 'Debug-validation-callback.txt'))) {
                throw 'Calibration continued after failed configuration checks'
            }
            if ($failAt -in @('checks', '--check-validation-callback')) {
                if (Test-Path -LiteralPath $startupLog) { throw 'Startup continued after an earlier verification failure' }
            } elseif ((Get-Content -Raw $startupLog) -notmatch 'startup') {
                throw 'Renderer startup output was not captured'
            }
            $presentationLog = Join-Path $renderLogs 'Debug-presentation.txt'
            if ($failAt -in @('checks', '--check-validation-callback', 'startup')) {
                if (Test-Path $presentationLog) { throw 'Presentation continued after an earlier failure' }
            } elseif (-not (Test-Path $presentationLog) -or (Get-Content -Raw $presentationLog) -notmatch '--check-presentation') {
                throw 'Renderer presentation was not exercised and captured'
            }
            $resourceLog = Join-Path $renderLogs 'Debug-resources.txt'
            if ($failAt -in @('checks', '--check-validation-callback', 'startup', '--check-presentation')) {
                if (Test-Path $resourceLog) { throw 'Resource checks continued after an earlier failure' }
            } elseif (-not (Test-Path $resourceLog) -or (Get-Content -Raw $resourceLog) -notmatch 'resources') {
                throw 'GPU resources were not exercised and captured'
            }
            $shaderLog = Join-Path $renderLogs 'Debug-shaders.txt'
            if ($failAt -in @('checks', '--check-validation-callback', 'startup', '--check-presentation', 'resources')) {
                if (Test-Path $shaderLog) { throw 'Shader checks continued after an earlier failure' }
            } elseif (-not (Test-Path $shaderLog) -or (Get-Content -Raw $shaderLog) -notmatch 'shaders') {
                throw 'GPU shaders were not exercised and captured'
            }
            $manifestLog = Join-Path $renderLogs 'Debug-shader-manifest.json'
            if ($failAt -ne '') {
                if (Test-Path $manifestLog) { throw 'Shader metadata was captured after a verification failure' }
            } else {
                if (-not (Test-Path $manifestLog) -or (Get-Content -Raw $manifestLog) -cne (Get-Content -Raw (Join-Path $shaderPackage 'manifest.json'))) {
                    throw 'Actual shader compiler identity was not retained'
                }
                foreach ($shader in @('cull', 'pbr-vertex', 'pbr-fragment')) {
                    if (-not (Test-Path (Join-Path $renderLogs "Debug-$shader.reflection.json"))) {
                        throw 'Shader reflection was not retained'
                    }
                }
            }
        }
        Remove-Item Env:NGEN_RENDER_ENV, Env:NGEN_PROBE_FAIL_AT
    }
    $sdkDefinition = $ast.Find({
        param($node)
        $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -eq 'Get-NgenVulkanSdkRoot'
    }, $true)
    if (-not $sdkDefinition) { throw 'Vulkan SDK environment discovery boundary not found' }
    . ([scriptblock]::Create($sdkDefinition.Extent.Text))
    $sdkChild = Join-Path $directory 'sdk environment child.ps1'
    Set-Content -LiteralPath $sdkChild -Value @'
param([string]$Expected)
if ([string]$env:VULKAN_SDK -cne $Expected) { exit 11 }
Write-Output 'SDK environment inherited'
'@
    $oldSdk = $env:VULKAN_SDK
    try {
        foreach ($case in @(
            @('', '', 'C:\Vulkan SDK\1.4.363.1', 'C:\Vulkan SDK\1.4.363.1'),
            @('', 'D:\User SDK', 'C:\Machine SDK', 'D:\User SDK'),
            @('E:\Process SDK', 'D:\User SDK', 'C:\Machine SDK', 'E:\Process SDK'),
            @('', '', '', '')
        )) {
            $env:VULKAN_SDK = Get-NgenVulkanSdkRoot -ProcessRoot $case[0] -UserRoot $case[1] -MachineRoot $case[2]
            Invoke-NgenNative -FilePath $pwsh -Arguments @(
                '-NoProfile', '-File', $sdkChild, $case[3]
            ) -LogPath $log
        }
    } finally {
        $env:VULKAN_SDK = $oldSdk
    }
    $profileDefinition = $ast.Find({
        param($node)
        $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -eq 'Get-NgenClangProfile'
    }, $true)
    if (-not $profileDefinition) { throw 'Clang profile boundary not found' }
    . ([scriptblock]::Create($profileDefinition.Extent.Text))
    $compilerPath = 'C:\Program Files\LLVM\bin\clang-cl.exe'
    $versionText = "clang version 23.1.4`nTarget: x86_64-pc-windows-msvc`nThread model: posix"
    $profile = Join-Path $directory 'clang profile'
    Get-NgenClangProfile -CompilerPath $compilerPath -VersionText $versionText -ToolVersion '14.44.35207' |
        Set-Content -LiteralPath $profile
    foreach ($config in @('Debug', 'Release')) {
        $reported = & conan profile show -pr:h $profile -pr:b (Join-Path $PSScriptRoot '../profiles/windows-msvc') -s:h "build_type=$config" -s:b "build_type=$config" --format json 2> $log
        if ($LASTEXITCODE -ne 0) { throw "Conan rejected the generated profile: $(Get-Content -Raw $log)" }
        $data = ($reported | Out-String) | ConvertFrom-Json
        $settings = $data.host.settings
        if ($settings.compiler -ne 'clang' -or $settings.'compiler.version' -ne '23' -or
            $settings.'compiler.cppstd' -ne '20' -or $settings.'compiler.runtime' -ne 'dynamic' -or
            $settings.'compiler.runtime_version' -ne 'v144' -or $settings.'compiler.runtime_type' -ne $config -or
            $data.build.settings.compiler -ne 'msvc' -or $data.build.settings.'compiler.runtime_type' -ne $config -or
            $data.host.conf.'tools.build:compiler_executables'.cpp -cne $compilerPath) {
            throw 'Generated Clang/MSVC host/build profiles lost compiler identity, CRT compatibility or executable path'
        }
    }
    foreach ($bad in @(
        @{ VersionText = 'not a compiler version'; ToolVersion = '14.44.35207' },
        @{ VersionText = "clang version 23.1.4`nTarget: i686-pc-windows-msvc"; ToolVersion = '14.44.35207' },
        @{ VersionText = $versionText; ToolVersion = 'unknown' }
    )) {
        $failed = $false
        try { Get-NgenClangProfile -CompilerPath $compilerPath @bad | Out-Null } catch { $failed = $true }
        if (-not $failed) { throw 'Unverified compiler/runtime identity was accepted' }
    }

    $failed = $false
    try {
        Invoke-NgenNative -FilePath (Join-Path $directory 'missing.exe') -LogPath $log
    } catch {
        $failed = $true
    }
    if (-not $failed) { throw 'A missing executable was reported as success' }
    Write-Host 'PASS: process/output boundaries, Clang/MSVC profiles, GPU/renderer checks, SDK environment inheritance and input/platform checks'
} finally {
    Remove-Item -LiteralPath $directory -Recurse -Force
}
# Expected failing subprocesses must not become the CI shell's final exit code.
exit 0
