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
    Write-Host 'PASS: process/output boundaries, Clang/MSVC profiles, GPU capture validation and input/platform checks'
} finally {
    Remove-Item -LiteralPath $directory -Recurse -Force
}
# Expected failing subprocesses must not become the CI shell's final exit code.
exit 0
