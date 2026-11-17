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
    if (-not $IsWindows) {
        & $pwsh -NoProfile -File $scriptPath -Scene *> $log
        if ($LASTEXITCODE -ne 2 -or (Get-Content -Raw $log) -notmatch 'requires Windows') { throw 'Scene mode did not reach the Windows prerequisite check' }
    }
    if (-not $IsWindows) {
        & $pwsh -NoProfile -File $scriptPath -ScenePackage 'not present.ngas' *> $log
        if ($LASTEXITCODE -ne 2 -or (Get-Content -Raw $log) -notmatch 'requires Windows') { throw 'Package scene mode did not reach the Windows prerequisite check' }
    }
    if (-not $IsWindows) {
        & $pwsh -NoProfile -File $scriptPath -Inspector -ScenePackage 'not present.ngas' *> $log
        if ($LASTEXITCODE -ne 2 -or (Get-Content -Raw $log) -notmatch 'requires Windows') { throw 'Inspector mode did not reach the Windows prerequisite check' }
    }
    if (-not $IsWindows) {
        & $pwsh -NoProfile -File $scriptPath -MeasureWorkload 'existing Release directory' -ScenePackage 'existing scene.ngas' *> $log
        if ($LASTEXITCODE -ne 2 -or (Get-Content -Raw $log) -notmatch 'requires Windows') { throw 'Measurement-only mode did not reach the Windows prerequisite check' }
    }
    & $pwsh -NoProfile -File $scriptPath -GpuInfo -MeasureWorkload 'existing Release directory' -ScenePackage 'existing scene.ngas' *> $log
    if ($LASTEXITCODE -eq 0 -or $LASTEXITCODE -eq 2) { throw 'GPU capability capture and measurement-only modes were mixed' }
    & $pwsh -NoProfile -File $scriptPath -MeasureWorkload 'existing Release directory' -ScenePackage 'existing scene.ngas' -Compiler MSVC *> $log
    if ($LASTEXITCODE -eq 0 -or $LASTEXITCODE -eq 2) { throw 'Measurement-only mode accepted a compiler/build selection' }
    & $pwsh -NoProfile -File $scriptPath -GpuInfo -Inspector *> $log
    if ($LASTEXITCODE -eq 0 -or $LASTEXITCODE -eq 2) { throw 'GPU capture and inspector builds were mixed' }
    & $pwsh -NoProfile -File $scriptPath -GpuInfo -ScenePackage 'not present.ngas' *> $log
    if ($LASTEXITCODE -eq 0 -or $LASTEXITCODE -eq 2) { throw 'GPU capture and package scene builds were mixed' }
    & $pwsh -NoProfile -File $scriptPath -GpuInfo -Scene *> $log
    if ($LASTEXITCODE -eq 0 -or $LASTEXITCODE -eq 2) { throw 'GPU capture and scene builds were mixed' }
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
        $workloadDefinition = $ast.Find({
            param($node)
            $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
            $node.Name -eq 'Invoke-NgenWorkload'
        }, $true)
        if (-not $workloadDefinition) { throw 'Workload process/memory boundary not found' }
        . ([scriptblock]::Create($workloadDefinition.Extent.Text))
        $identityDefinition = $ast.Find({
            param($node)
            $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
            $node.Name -eq 'Get-NgenSceneIdentity'
        }, $true)
        if (-not $identityDefinition) { throw 'Checked package identity boundary not found' }
        . ([scriptblock]::Create($identityDefinition.Extent.Text))
        $renderBuild = Join-Path $directory 'renderer build'
        New-Item -ItemType Directory $renderBuild | Out-Null
        Set-Content (Join-Path $renderBuild 'conanrun.ps1') '$env:NGEN_RENDER_ENV = "active"'
        Set-Content (Join-Path $renderBuild 'deactivate_conanrun.ps1') '$env:NGEN_RENDER_ENV = "before"'
        $shaderPackage = Join-Path $renderBuild 'shaders/cooked'
        New-Item -ItemType Directory $shaderPackage | Out-Null
        Set-Content (Join-Path $shaderPackage 'manifest.json') '{"schema":1,"compiler":{"sha256":"fixture compiler identity"}}'
        foreach ($shader in @('cull', 'pbr-vertex', 'pbr-fragment', 'shadow-vertex', 'shadow-fragment', 'post-vertex', 'post-fragment', 'overlay-vertex', 'overlay-fragment')) {
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
  */inspector_probe.exe) mode=inspector ;;
  */scene_workload.exe)
    [ -f "$1" ] || exit 9
    mode=workload-steady
    for argument in "$@"; do
      printf 'workload argument: %s\n' "$argument"
      case "$argument" in --validation) mode=workload-inspection ;; --no-resize) mode=workload-stream ;; esac
    done
    for argument in "$@"; do
      case "$argument" in
        --capture=*) [ "$NGEN_PROBE_FAIL_AT" = no-workload-capture ] || printf 'fixture UI capture' > "${argument#--capture=}" ;;
        --report=*)
          report_path=${argument#--report=}
          [ "$NGEN_PROBE_FAIL_AT" = no-workload-report ] || printf 'frame,frame_ns,host_ns\n0,0,%s\n' "$(python3 -c 'import time; print(time.monotonic_ns())')" > "$report_path"
          ;;
      esac
    done
    sleep 0.2
    if [ -n "${report_path:-}" ] && [ "$NGEN_PROBE_FAIL_AT" != no-workload-report ]; then
      printf '1,0,%s\n' "$(python3 -c 'import time; print(time.monotonic_ns())')" >> "$report_path"
    fi
    ;;
  */scene_probe.exe)
    mode=scene
    if [ "$#" -gt 0 ]; then
      [ "$#" -le 2 ] && [ -f "$1" ] || exit 9
      mode=scene-package
      printf 'package argument: %s\n' "$1"
      if [ "$#" -eq 2 ] && [ "$NGEN_PROBE_FAIL_AT" != no-capture ]; then
        printf 'fixture capture' > "$2"
        printf 'capture argument: %s\n' "$2"
      fi
      if [ "$NGEN_PACKAGE_MUTATE" = 1 ]; then printf 'changed' >> "$1"; fi
    fi
    ;;
esac
printf '%s\n' "$mode"
[ "$mode" != "$NGEN_PROBE_FAIL_AT" ] || exit 7
exit 0
'@
        $resourceProbe = Join-Path $renderBuild 'resource_probe.exe'
        Copy-Item -LiteralPath $probe -Destination $resourceProbe
        $shaderProbe = Join-Path $renderBuild 'shader_gpu_probe.exe'
        Copy-Item -LiteralPath $probe -Destination $shaderProbe
        $sceneProbe = Join-Path $renderBuild 'scene_probe.exe'
        Copy-Item -LiteralPath $probe -Destination $sceneProbe
        $inspectorProbe = Join-Path $renderBuild 'inspector_probe.exe'
        $workloadProbe = Join-Path $renderBuild 'scene_workload.exe'
        Copy-Item -LiteralPath $probe -Destination $inspectorProbe
        Copy-Item -LiteralPath $probe -Destination $workloadProbe
        & chmod +x $probe $resourceProbe $shaderProbe $sceneProbe $inspectorProbe $workloadProbe
        if ($LASTEXITCODE -ne 0) { throw 'Could not prepare renderer subprocess fixture' }
        foreach ($failAt in @('', 'checks', '--check-validation-callback', 'startup', '--check-presentation', 'resources', 'shaders', 'scene')) {
            $renderLogs = Join-Path $directory ('render logs ' + [guid]::NewGuid())
            New-Item -ItemType Directory $renderLogs | Out-Null
            $env:NGEN_RENDER_ENV = 'before'
            $env:NGEN_PROBE_FAIL_AT = $failAt
            $failed = $false
            $checks = {
                Invoke-NgenNative -FilePath $probe -Arguments @('checks') -LogPath (Join-Path $renderLogs 'Debug-checks.txt')
            }
            try {
                Invoke-NgenRenderer -BuildDirectory $renderBuild -LogDirectory $renderLogs -Configuration Debug -Checks $checks -Scene
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
            $sceneLog = Join-Path $renderLogs 'Debug-scene.txt'
            if ($failAt -in @('checks', '--check-validation-callback', 'startup', '--check-presentation', 'resources', 'shaders')) {
                if (Test-Path $sceneLog) { throw 'Scene checks continued after an earlier failure' }
            } elseif (-not (Test-Path $sceneLog) -or (Get-Content -Raw $sceneLog) -notmatch 'scene') {
                throw 'Checked scene process was not exercised and captured'
            }
            $manifestLog = Join-Path $renderLogs 'Debug-shader-manifest.json'
            if ($failAt -ne '') {
                if (Test-Path $manifestLog) { throw 'Shader metadata was captured after a verification failure' }
            } else {
                if (-not (Test-Path $manifestLog) -or (Get-Content -Raw $manifestLog) -cne (Get-Content -Raw (Join-Path $shaderPackage 'manifest.json'))) {
                    throw 'Actual shader compiler identity was not retained'
                }
                foreach ($shader in @('cull', 'pbr-vertex', 'pbr-fragment', 'shadow-vertex', 'shadow-fragment', 'post-vertex', 'post-fragment', 'overlay-vertex', 'overlay-fragment')) {
                    if (-not (Test-Path (Join-Path $renderLogs "Debug-$shader.reflection.json"))) {
                        throw 'Shader reflection was not retained'
                    }
                }
            }
        }
        $scenePackage = Join-Path $directory 'checked ship with spaces.ngas'
        foreach ($failAt in @('', 'scene', 'scene-package', 'mutate-package', 'no-capture')) {
            [IO.File]::WriteAllText($scenePackage, 'checked package fixture bytes')
            $expectedHash = (Get-FileHash -LiteralPath $scenePackage -Algorithm SHA256).Hash.ToLowerInvariant()
            $packageLogs = Join-Path $directory ('scene package logs ' + [guid]::NewGuid())
            New-Item -ItemType Directory $packageLogs | Out-Null
            $env:NGEN_PROBE_FAIL_AT = $failAt
            $env:NGEN_PACKAGE_MUTATE = if ($failAt -eq 'mutate-package') { '1' } else { '0' }
            $failed = $false
            try {
                Invoke-NgenRenderer -BuildDirectory $renderBuild -LogDirectory $packageLogs -Configuration Release -Checks { } -ScenePackage $scenePackage
            } catch { $failed = $true }
            if ($failAt -eq '' -and -not (Test-Path (Join-Path $packageLogs 'Release-scene-package.bmp'))) { throw 'A requested native scene capture was not retained' }
            if ($failed -ne ($failAt -ne '')) { throw 'Explicit checked package failure, mutation or missing capture was lost or falsely reported' }
            if ($env:NGEN_RENDER_ENV -ne 'before') { throw 'Package verification did not restore the runtime environment' }
            $packageLog = Join-Path $packageLogs 'Release-scene-package.txt'
            if ($failAt -eq 'scene') {
                if (Test-Path $packageLog) { throw 'Package verification continued after failed built-in scene checks' }
            } else {
                if (-not (Test-Path $packageLog) -or (Get-Content -Raw $packageLog) -notmatch 'scene-package' -or
                    -not (Get-Content -Raw $packageLog).Contains($scenePackage)) { throw 'Explicit checked package was not executed as one native argument and captured separately' }
                $identity = Get-Content -Raw (Join-Path $packageLogs 'Release-scene-package.json') | ConvertFrom-Json
                if ($identity.schema -ne 1 -or $identity.name -cne 'checked ship with spaces.ngas' -or
                    $identity.bytes -ne 29 -or $identity.sha256 -cne $expectedHash) { throw 'Actual package bytes were not identified before native execution' }
            }
            if ($failAt -ne '' -and (Test-Path (Join-Path $packageLogs 'Release-shader-manifest.json'))) { throw 'Verification continued after a package failure' }
        }
        Remove-Item Env:NGEN_PACKAGE_MUTATE
        $inspectorLogs = Join-Path $directory 'inspector logs'
        New-Item -ItemType Directory $inspectorLogs | Out-Null
        $env:NGEN_PROBE_FAIL_AT = ''
        Invoke-NgenRenderer -BuildDirectory $renderBuild -LogDirectory $inspectorLogs -Configuration Release -Checks { } -Inspector -ScenePackage $scenePackage
        if (-not (Test-Path (Join-Path $inspectorLogs 'Release-inspector.txt'))) { throw 'Inspector mode did not execute the actual inspector process' }
        foreach ($mode in @('inspection', 'steady', 'stream')) {
            foreach ($extension in @('txt', 'bmp', 'csv', 'working-set.json', 'artifacts.json')) {
                if (-not (Test-Path (Join-Path $inspectorLogs "Release-$mode.$extension"))) { throw 'Combined workload did not retain capture/report/process-memory evidence' }
            }
        }
        foreach ($mode in @('inspection', 'steady', 'stream')) {
            $text = Get-Content -Raw (Join-Path $inspectorLogs "Release-$mode.txt")
            if (-not $text.Contains($scenePackage) -or $text -notmatch '--inspector') { throw 'Inspector workload lost its single package argument or frontend flag' }
            if ($mode -eq 'inspection' -and ($text -notmatch '--validation' -or $text -notmatch '--frames=1200')) { throw 'Validation walkthrough was not selected' }
            if ($mode -ne 'inspection' -and $text -match '--validation') { throw 'Performance workload unexpectedly enabled validation' }
            if ($mode -eq 'stream' -and ($text -notmatch '--no-resize' -or $text -notmatch '--reload-cycles=4')) { throw 'Fixed-resolution streaming scenario was not selected' }
            $memory = Get-Content -Raw (Join-Path $inspectorLogs "Release-$mode.working-set.json") | ConvertFrom-Json
            if ($memory.exit_code -ne 0 -or $memory.timed_out -or $memory.observed_peak_working_set_bytes -le 0) { throw 'Actual child working-set observations were not retained' }
        }
        foreach ($failAt in @('inspector', 'workload-inspection', 'workload-steady', 'workload-stream', 'no-workload-capture', 'no-workload-report')) {
            $failedLogs = Join-Path $directory ('inspector failure ' + [guid]::NewGuid())
            New-Item -ItemType Directory $failedLogs | Out-Null
            $env:NGEN_PROBE_FAIL_AT = $failAt
            $failed = $false
            try { Invoke-NgenRenderer -BuildDirectory $renderBuild -LogDirectory $failedLogs -Configuration Release -Checks { } -Inspector -ScenePackage $scenePackage } catch { $failed = $true }
            if (-not $failed) { throw 'Inspector subprocess failure or missing output was falsely accepted' }
            if ($env:NGEN_RENDER_ENV -ne 'before') { throw 'Inspector failure did not restore the Conan runtime environment' }
            if (Test-Path (Join-Path $failedLogs 'Release-shader-manifest.json')) { throw 'Verification continued after inspector workload failure' }
        }
        $ownerLogs = Join-Path $directory 'inspector existing output'
        New-Item -ItemType Directory $ownerLogs | Out-Null
        $ownerCapture = Join-Path $ownerLogs 'Release-inspection.bmp'
        Set-Content -LiteralPath $ownerCapture -Value 'owner capture must survive'
        $beforeOwner = Get-FileHash -LiteralPath $ownerCapture
        $env:NGEN_PROBE_FAIL_AT = ''
        $failed = $false
        try { Invoke-NgenRenderer -BuildDirectory $renderBuild -LogDirectory $ownerLogs -Configuration Release -Checks { } -Inspector -ScenePackage $scenePackage } catch { $failed = $true }
        if (-not $failed -or (Get-FileHash -LiteralPath $ownerCapture).Hash -cne $beforeOwner.Hash) { throw 'Inspector evidence overwrote an existing owner capture' }
        $timeoutLog = Join-Path $directory 'workload timeout.txt'
        $failed = $false
        try { Invoke-NgenWorkload -FilePath $pwsh -Arguments @('-NoProfile', '-Command', 'Start-Sleep -Seconds 3') -LogPath $timeoutLog -MemoryPath "$timeoutLog.json" -TimeoutMilliseconds 1000 } catch { $failed = $_.Exception.Message -match 'deadline' }
        if (-not $failed -or -not (Get-Content -Raw "$timeoutLog.json" | ConvertFrom-Json).timed_out) { throw 'Workload timeout was not retained as failure' }
        # Native PDH process-memory reads are the OS boundary; the observed child is real.
        $script:gpuFailure = ''
        $script:gpuReadCalls = 0
        $script:gpuFramePhasePath = ''
        $script:gpuMeasuredReads = 0
        function Get-CimInstance { throw 'The workload must use the direct PDH transport, not CIM.' }
        function Get-NgenPdhProcessMemory {
            param([string]$FilePath, [int]$ProcessId, [ref]$Reader)
            if ($ProcessId -le 0 -or [IO.Path]::GetFileName($FilePath) -cne 'gpu_memory_observer.exe') {
                throw 'GPU reader is not scoped to its owned child and declared native helper.'
            }
            $target = [string]$ProcessId
            if ($script:gpuFailure -eq 'unavailable') { throw 'fixture CIM class unavailable' }
            if ($script:gpuFailure -eq 'cim-error') {
                $exception = [Microsoft.Management.Infrastructure.CimException]::new('fixture CIM provider failure')
                $record = [Management.Automation.ErrorRecord]::new($exception, ('NgenFixtureCimFailure' + ('x' * 600)),
                    [Management.Automation.ErrorCategory]::ResourceUnavailable, $null)
                Write-Error -ErrorRecord $record -ErrorAction Stop
            }
            if ($script:gpuFailure -eq 'late-query') {
                if (++$script:gpuReadCalls -eq 1) { Start-Sleep -Milliseconds 1300 }
            }
            if ($script:gpuFailure -eq 'slow-query') {
                if (++$script:gpuReadCalls -eq 2) { Start-Sleep -Milliseconds 1300 }
            }
            if ($script:gpuFailure -in @('scoped-window', 'scoped-gap')) {
                ++$script:gpuReadCalls
                $phase = if (Test-Path $script:gpuFramePhasePath) { (Get-Content -Raw $script:gpuFramePhasePath).Trim() } else { 'startup' }
                if ($phase -ne 'measuring' -and $script:gpuReadCalls -gt 1) { return }
                if ($phase -eq 'measuring' -and $script:gpuFailure -eq 'scoped-gap' -and
                    ++$script:gpuMeasuredReads -eq 2) { return }
            }
            if ($script:gpuFailure -in @('disappear', 'foreign-after-valid')) {
                ++$script:gpuReadCalls
                if ($script:gpuReadCalls -gt 1) {
                    if ($script:gpuFailure -eq 'foreign-after-valid') {
                        [pscustomobject]@{ Name = 'pid_2147483647_luid_0x0_0x1_phys_0'; DedicatedUsage = 1048576L; SharedUsage = 0L; TotalCommitted = 1048576L }
                    }
                    return
                }
            }
            if ($script:gpuFailure -in @('over-budget-shared', 'over-budget-committed', 'zero')) {
                [pscustomobject]@{ Name = "pid_${target}_luid_0x0_0x1_phys_0"
                    DedicatedUsage = if ($script:gpuFailure -eq 'over-budget-shared') { 536870912L } else { 0L }
                    SharedUsage = if ($script:gpuFailure -eq 'over-budget-shared') { 536870913L } else { 0L }
                    TotalCommitted = if ($script:gpuFailure -eq 'over-budget-committed') { 1073741825L } else { 0L }
                }
                return
            }
            if ($script:gpuFailure -eq 'malformed-instance') {
                [pscustomobject]@{ Name = "pid_${target}_unsupported_instance"; DedicatedUsage = 1048576L; SharedUsage = 2097152L; TotalCommitted = 4194304L }
            }
            @(
                [pscustomobject]@{ Name = "pid_${target}_luid_0x0_0x1_phys_0"; DedicatedUsage = 1048576L; SharedUsage = 2097152L; TotalCommitted = 4194304L },
                [pscustomobject]@{ Name = "pid_${target}_luid_0x0_0x2_phys_0"; DedicatedUsage = 4194304L; SharedUsage = 8388608L; TotalCommitted = 16777216L },
                [pscustomobject]@{ Name = 'pid_2147483647_luid_0x0_0x1_phys_0'; DedicatedUsage = 1073741824L; SharedUsage = 1073741824L; TotalCommitted = 2147483648L }
            )
        }
        try {
            $gpuLog = Join-Path $directory 'process gpu memory.txt'
            Invoke-NgenWorkload -FilePath $pwsh -Arguments @('-NoProfile', '-Command', 'Start-Sleep -Milliseconds 1200') -LogPath $gpuLog -MemoryPath "$gpuLog.json" -GpuMemory
            $observation = Get-Content -Raw "$gpuLog.json" | ConvertFrom-Json
            if ($observation.gpu_memory.status -cne 'observed' -or
                $observation.gpu_memory.peak_dedicated_bytes -ne 5242880 -or
                $observation.gpu_memory.peak_shared_bytes -ne 10485760 -or
                $observation.gpu_memory.peak_dedicated_plus_shared_bytes -ne 15728640 -or
                $observation.gpu_memory.peak_total_committed_bytes -ne 20971520 -or
                $observation.gpu_memory.samples.Count -lt 2) { throw 'Requested per-process GPU memory was not observed, or foreign process counters contaminated it' }
            if ($observation.gpu_memory.parent_qpc_frequency -le 0 -or
                $observation.gpu_memory.parent_qpc_started_ticks -le 0 -or
                $observation.gpu_memory.parent_qpc_ended_ticks -le $observation.gpu_memory.parent_qpc_started_ticks) { throw 'GPU observations lack a monotonic clock window for native-frame correlation' }
            foreach ($sample in $observation.gpu_memory.samples) {
                if ($sample.instances -ne 2 -or $sample.elapsed_ms -lt 0 -or
                    $sample.timestamp_ticks -lt $observation.gpu_memory.parent_qpc_started_ticks -or
                    $sample.timestamp_ticks -gt $observation.gpu_memory.parent_qpc_ended_ticks) { throw 'GPU memory samples lost adapter cardinality or observation time' }
            }
            $script:gpuFailure = 'unavailable'
            $gpuMissing = Join-Path $directory 'unavailable gpu memory.txt'
            $failed = $false
            try {
                Invoke-NgenWorkload -FilePath $pwsh -Arguments @('-NoProfile', '-Command', 'Start-Sleep -Milliseconds 1200') -LogPath $gpuMissing -MemoryPath "$gpuMissing.json" -GpuMemory
            } catch { $failed = $_.Exception.Message -match 'GPU memory' }
            $missing = Get-Content -Raw "$gpuMissing.json" | ConvertFrom-Json
            if (-not $failed -or $missing.gpu_memory.status -cne 'unavailable' -or
                $missing.gpu_memory.error_message -cne 'fixture CIM class unavailable' -or
                $null -eq $missing.gpu_memory.error_hresult -or
                $null -ne $missing.gpu_memory.error_details.native_error_code -or
                $null -ne $missing.gpu_memory.error_details.status_code -or
                $null -ne $missing.gpu_memory.peak_dedicated_plus_shared_bytes -or
                $missing.gpu_memory.samples.Count -ne 0 -or $missing.exit_code -ne 0) { throw 'Unavailable GPU counters were accepted as measured zero, or diagnostics/actual child result were lost' }
            $script:gpuFailure = 'cim-error'
            $cimLog = Join-Path $directory 'structured CIM failure.txt'
            $failed = $false
            try {
                Invoke-NgenWorkload -FilePath $pwsh -Arguments @('-NoProfile', '-Command', 'Start-Sleep -Milliseconds 600') -LogPath $cimLog -MemoryPath "$cimLog.json" -GpuMemory
            } catch { $failed = $_.Exception.Message -match 'GPU memory' }
            $cimFailure = Get-Content -Raw "$cimLog.json" | ConvertFrom-Json
            $detail = $cimFailure.gpu_memory.error_details
            if (-not $failed -or $cimFailure.gpu_memory.status -cne 'unavailable' -or
                $null -ne $cimFailure.gpu_memory.within_1gib_budget -or
                $cimFailure.gpu_memory.samples.Count -ne 0 -or $cimFailure.exit_code -ne 0 -or
                $null -eq $detail -or $detail.fully_qualified_error_id -notlike 'NgenFixtureCimFailure*' -or
                $detail.fully_qualified_error_id.Length -ne 512 -or
                $detail.category -cne 'ResourceUnavailable' -or $detail.native_error_code -cne 'Ok' -or
                $detail.status_code -ne 0) { throw 'Structured CIM failure diagnostics were lost or accepted as GPU memory proof' }
            foreach ($budgetCase in @('over-budget-shared', 'over-budget-committed')) {
                $script:gpuFailure = $budgetCase
                $gpuBudget = Join-Path $directory "$budgetCase.txt"
                $failed = $false
                try {
                    Invoke-NgenWorkload -FilePath $pwsh -Arguments @('-NoProfile', '-Command', 'Start-Sleep -Milliseconds 600') -LogPath $gpuBudget -MemoryPath "$gpuBudget.json" -GpuMemory
                } catch { $failed = $_.Exception.Message -match 'GPU memory.*1 GiB' }
                $budget = Get-Content -Raw "$gpuBudget.json" | ConvertFrom-Json
                if (-not $failed -or $budget.gpu_memory.within_1gib_budget -ne $false -or
                    $budget.gpu_memory.status -cne 'observed' -or $budget.exit_code -ne 0) { throw 'An observed dedicated/shared or committed GPU memory budget failure was accepted, or its evidence was lost' }
            }
            $script:gpuFailure = 'malformed-instance'
            $gpuMalformed = Join-Path $directory 'malformed process GPU instance.txt'
            $failed = $false
            try {
                Invoke-NgenWorkload -FilePath $pwsh -Arguments @('-NoProfile', '-Command', 'Start-Sleep -Milliseconds 600') -LogPath $gpuMalformed -MemoryPath "$gpuMalformed.json" -GpuMemory
            } catch { $failed = $_.Exception.Message -match 'GPU memory' }
            $malformed = Get-Content -Raw "$gpuMalformed.json" | ConvertFrom-Json
            if (-not $failed -or $malformed.gpu_memory.status -eq 'observed') { throw 'A malformed own-process GPU instance was silently omitted from an accepted memory total' }
            $script:gpuFailure = 'zero'
            $gpuZero = Join-Path $directory 'zero GPU counter trace.txt'
            $failed = $false
            try {
                Invoke-NgenWorkload -FilePath $pwsh -Arguments @('-NoProfile', '-Command', 'Start-Sleep -Milliseconds 600') -LogPath $gpuZero -MemoryPath "$gpuZero.json" -GpuMemory
            } catch { $failed = $_.Exception.Message -match 'GPU memory' }
            $zero = Get-Content -Raw "$gpuZero.json" | ConvertFrom-Json
            if (-not $failed -or $zero.gpu_memory.status -eq 'observed' -or $null -ne $zero.gpu_memory.within_1gib_budget) { throw 'An entirely zero provider trace was accepted as GPU-workload memory proof' }
            foreach ($gapCase in @('disappear', 'foreign-after-valid')) {
                $script:gpuFailure = $gapCase
                $script:gpuReadCalls = 0
                $gpuGap = Join-Path $directory "$gapCase.txt"
                $failed = $false
                try {
                    Invoke-NgenWorkload -FilePath $pwsh -Arguments @('-NoProfile', '-Command', 'Start-Sleep -Milliseconds 1800') -LogPath $gpuGap -MemoryPath "$gpuGap.json" -GpuMemory
                } catch { $failed = $_.Exception.Message -match 'GPU memory' }
                $gap = Get-Content -Raw "$gpuGap.json" | ConvertFrom-Json
                if (-not $failed -or $gap.gpu_memory.status -cne 'incomplete' -or
                    $null -ne $gap.gpu_memory.within_1gib_budget -or $gap.gpu_memory.queries.Count -lt 2 -or
                    @($gap.gpu_memory.queries | Where-Object status -eq 'no_own_instances').Count -eq 0) { throw 'A valid-then-empty or foreign-only GPU trace was accepted, or missing query attempts were lost' }
            }
            $frameChild = Join-Path $directory 'measured frame window.ps1'
            Set-Content -LiteralPath $frameChild -Value @'
param($report, $phase)
function TickNs { [long]([decimal][Diagnostics.Stopwatch]::GetTimestamp() * 1000000000 / [Diagnostics.Stopwatch]::Frequency) }
Set-Content -LiteralPath $phase 'startup'
Start-Sleep -Milliseconds 1100
Set-Content -LiteralPath $phase 'measuring'
$begin = TickNs
Start-Sleep -Milliseconds 1700
$end = TickNs
Set-Content -LiteralPath $phase 'shutdown'
# Flight adds bounded control/camera/audio diagnostics to the same frame clock.
$columns = (0..56 | ForEach-Object { "diagnostic$_" }) -join ','
$values = (0..56 | ForEach-Object { '0' }) -join ','
"frame,frame_ns,host_ns,$columns`n0,0,$begin,$values`n1,0,$end,$values" | Set-Content -LiteralPath $report
Start-Sleep -Milliseconds 900
'@
            $script:gpuFailure = 'scoped-window'
            $script:gpuReadCalls = 0
            $script:gpuFramePhasePath = Join-Path $directory 'GPU child phase.txt'
            $scopedReport = Join-Path $directory 'native frame window.csv'
            $scopedLog = Join-Path $directory 'GPU measured window.txt'
            try {
                Invoke-NgenWorkload -FilePath $pwsh -Arguments @('-NoProfile', '-File', $frameChild, $scopedReport, $script:gpuFramePhasePath) -LogPath $scopedLog -MemoryPath "$scopedLog.json" -GpuMemory -FrameReport $scopedReport
            } catch {
                $rejected = Get-Content -Raw "$scopedLog.json" | ConvertFrom-Json
                if ($rejected.gpu_memory.coverage.error -ceq 'InvalidOrUncorrelatedNativeFrameReport') {
                    throw 'The bounded flight-width native clock report was rejected.'
                }
                throw
            }
            $scoped = Get-Content -Raw "$scopedLog.json" | ConvertFrom-Json
            if ($scoped.gpu_memory.status -cne 'observed' -or -not $scoped.gpu_memory.coverage.complete -or
                $scoped.gpu_memory.coverage.scope -cne 'native_measured_frames' -or
                @($scoped.gpu_memory.queries | Where-Object status -eq 'no_own_instances').Count -eq 0) { throw 'Explicit native-frame coverage did not preserve startup/exit gaps separately from the measured window' }
            $script:gpuFailure = 'scoped-gap'
            $script:gpuReadCalls = 0
            $script:gpuMeasuredReads = 0
            $script:gpuFramePhasePath = Join-Path $directory 'gapped measured phase.txt'
            $gappedReport = Join-Path $directory 'gapped native frame window.csv'
            $gappedLog = Join-Path $directory 'GPU measured active gap.txt'
            $failed = $false
            try {
                Invoke-NgenWorkload -FilePath $pwsh -Arguments @('-NoProfile', '-File', $frameChild, $gappedReport, $script:gpuFramePhasePath) -LogPath $gappedLog -MemoryPath "$gappedLog.json" -GpuMemory -FrameReport $gappedReport
            } catch { $failed = $_.Exception.Message -match 'GPU memory' }
            $gapped = Get-Content -Raw "$gappedLog.json" | ConvertFrom-Json
            if (-not $failed -or $gapped.gpu_memory.coverage.complete -or
                $gapped.gpu_memory.coverage.missing_queries_in_window -lt 1 -or
                $null -ne $gapped.gpu_memory.within_1gib_budget) { throw 'A missing active measured-window query was exempted as startup or exit' }
            $script:gpuFailure = 'slow-query'
            $script:gpuReadCalls = 0
            $slowLog = Join-Path $directory 'GPU slow successful query.txt'
            $failed = $false
            try {
                Invoke-NgenWorkload -FilePath $pwsh -Arguments @('-NoProfile', '-Command', 'Start-Sleep -Milliseconds 2600') -LogPath $slowLog -MemoryPath "$slowLog.json" -GpuMemory
            } catch { $failed = $_.Exception.Message -match 'GPU memory' }
            $slow = Get-Content -Raw "$slowLog.json" | ConvertFrom-Json
            if (-not $failed -or $slow.gpu_memory.coverage.maximum_observed_gap_ms -le 1000 -or
                @($slow.gpu_memory.queries | Where-Object status -ne 'observed').Count -ne 0) { throw 'Slow successful queries hid an excessive GPU observation gap' }
            $script:gpuFailure = 'late-query'
            $script:gpuReadCalls = 0
            $lateLog = Join-Path $directory 'GPU observation deadline.txt'
            $failed = $false
            try {
                Invoke-NgenWorkload -FilePath $pwsh -Arguments @('-NoProfile', '-Command', 'Start-Sleep -Milliseconds 200') -LogPath $lateLog -MemoryPath "$lateLog.json" -GpuMemory -TimeoutMilliseconds 1000
            } catch { $failed = $_.Exception.Message -match 'deadline' }
            $late = Get-Content -Raw "$lateLog.json" | ConvertFrom-Json
            if (-not $failed -or -not $late.timed_out -or $late.exit_code -ne 0) { throw 'An observation overrun was hidden when the child exited during a CIM query' }
            $script:gpuFailure = ''
            $badClock = Join-Path $directory 'uncorrelated native clock.csv'
            Set-Content -LiteralPath $badClock -Value @('frame,frame_ns,host_ns', '0,0,1', '1,0,2')
            $badClockLog = Join-Path $directory 'GPU bad frame clock.txt'
            $failed = $false
            try {
                Invoke-NgenWorkload -FilePath $pwsh -Arguments @('-NoProfile', '-Command', 'Start-Sleep -Milliseconds 600') -LogPath $badClockLog -MemoryPath "$badClockLog.json" -GpuMemory -FrameReport $badClock
            } catch { $failed = $_.Exception.Message -match 'GPU memory' }
            $bad = Get-Content -Raw "$badClockLog.json" | ConvertFrom-Json
            if (-not $failed -or $bad.gpu_memory.coverage.complete -or -not $bad.gpu_memory.coverage.error) { throw 'Uncorrelated native frame clocks were accepted as GPU coverage' }
            $measurementDefinition = $ast.Find({
                param($node)
                $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
                $node.Name -eq 'Invoke-NgenMeasurement'
            }, $true)
            . ([scriptblock]::Create($measurementDefinition.Extent.Text))
            Set-Content -LiteralPath (Join-Path $renderBuild 'CMakeCache.txt') 'CMAKE_BUILD_TYPE:STRING=Release'
            $buildLogs = Join-Path $directory 'logs'
            New-Item -ItemType Directory $buildLogs | Out-Null
            Set-Content -LiteralPath (Join-Path $buildLogs 'revision.txt') '1f0a4b31a3db73f802513eab6edebd319f3c47cb'
            [IO.File]::WriteAllText((Join-Path $buildLogs 'working-tree.txt'), '')
            $measurementLogs = Join-Path $directory 'measurement only logs'
            New-Item -ItemType Directory $measurementLogs | Out-Null
            $env:NGEN_PROBE_FAIL_AT = ''
            Invoke-NgenMeasurement -BuildDirectory $renderBuild -LogDirectory $measurementLogs -ScenePackage $scenePackage
            foreach ($extension in @('txt', 'bmp', 'csv', 'working-set.json', 'artifacts.json', 'input.json')) {
                if (-not (Test-Path (Join-Path $measurementLogs "long-stream.$extension"))) { throw 'Measurement-only mode did not run and retain the existing workload without rebuilding' }
            }
            $measurement = Get-Content -Raw (Join-Path $measurementLogs 'long-stream.txt')
            if ($measurement -notmatch '--frames=3600' -or $measurement -notmatch '--reload-cycles=4' -or
                $measurement -notmatch '--no-resize' -or $measurement -match '--validation') { throw 'Memory measurement silently changed its declared longer fixed-resolution quality scenario' }
            if ($env:NGEN_RENDER_ENV -ne 'before') { throw 'Measurement-only runtime environment was not restored' }
            $measured = Get-Content -Raw (Join-Path $measurementLogs 'long-stream.working-set.json') | ConvertFrom-Json
            if ($measured.gpu_memory.status -cne 'observed' -or -not $measured.gpu_memory.coverage.complete -or
                $measured.gpu_memory.coverage.scope -cne 'native_measured_frames' -or
                $measured.gpu_memory.coverage.frame_rows -ne 2) { throw 'Measurement-only mode did not validate process GPU coverage against actual child frame clocks' }
            foreach ($failureCase in @('workload-stream', 'no-workload-capture', 'unavailable-gpu')) {
                $failureLogs = Join-Path $directory ("measurement failure $failureCase")
                New-Item -ItemType Directory $failureLogs | Out-Null
                $env:NGEN_PROBE_FAIL_AT = $failureCase
                $script:gpuFailure = if ($failureCase -eq 'unavailable-gpu') { 'unavailable' } else { '' }
                $failed = $false
                try { Invoke-NgenMeasurement -BuildDirectory $renderBuild -LogDirectory $failureLogs -ScenePackage $scenePackage } catch { $failed = $true }
                if (-not $failed -or $env:NGEN_RENDER_ENV -ne 'before' -or
                    -not (Test-Path (Join-Path $failureLogs 'long-stream.working-set.json'))) { throw 'Measurement failure was accepted, lost its observation or failed environment cleanup' }
            }
            $script:gpuFailure = ''
            $env:NGEN_PROBE_FAIL_AT = ''
            $ownedLogs = Join-Path $directory 'measurement owner output'
            New-Item -ItemType Directory $ownedLogs | Out-Null
            $owned = Join-Path $ownedLogs 'long-stream.bmp'
            [IO.File]::WriteAllText($owned, 'existing owner capture')
            $ownedHash = (Get-FileHash -LiteralPath $owned).Hash
            $failed = $false
            try { Invoke-NgenMeasurement -BuildDirectory $renderBuild -LogDirectory $ownedLogs -ScenePackage $scenePackage } catch { $failed = $true }
            if (-not $failed -or (Get-FileHash -LiteralPath $owned).Hash -cne $ownedHash -or
                (Test-Path (Join-Path $ownedLogs 'long-stream.input.json'))) { throw 'Measurement overwrote an owner artifact or launched before freshness admission' }
            foreach ($invalidBuild in @('Debug', 'dirty-record')) {
                $cacheType = if ($invalidBuild -eq 'Debug') { 'CMAKE_BUILD_TYPE:STRING=Debug' } else { 'CMAKE_BUILD_TYPE:STRING=Release' }
                Set-Content -LiteralPath (Join-Path $renderBuild 'CMakeCache.txt') $cacheType
                if ($invalidBuild -eq 'dirty-record') { Set-Content -LiteralPath (Join-Path $buildLogs 'working-tree.txt') ' M changed.cpp' }
                $invalidLogs = Join-Path $directory ("invalid measurement build $invalidBuild")
                New-Item -ItemType Directory $invalidLogs | Out-Null
                $failed = $false
                try { Invoke-NgenMeasurement -BuildDirectory $renderBuild -LogDirectory $invalidLogs -ScenePackage $scenePackage } catch { $failed = $true }
                if (-not $failed -or (Test-Path (Join-Path $invalidLogs 'long-stream.input.json'))) { throw 'Measurement accepted Debug or a dirty declared build as clean Release' }
            }
        } finally {
            Remove-Item Function:Get-NgenPdhProcessMemory, Function:Get-CimInstance
            Remove-Variable gpuFailure, gpuReadCalls, gpuFramePhasePath, gpuMeasuredReads -Scope Script
        }
        $emptyPackage = Join-Path $directory 'empty.ngas'
        [IO.File]::WriteAllText($emptyPackage, '')
        $oversized = Join-Path $directory 'oversized.ngas'
        $stream = [IO.File]::Create($oversized)
        try { $stream.SetLength(268435457) } finally { $stream.Dispose() }
        $link = Join-Path $directory 'link.ngas'
        New-Item -ItemType SymbolicLink -Path $link -Target $scenePackage | Out-Null
        foreach ($bad in @($directory, $emptyPackage, $oversized, $link, (Join-Path $directory 'missing.ngas'), $child)) {
            $failed = $false
            try { Get-NgenSceneIdentity -Path $bad | Out-Null } catch { $failed = $true }
            if (-not $failed) { throw 'An invalid, oversized, missing, linked or wrong-format package path was accepted' }
        }
        $rendererOnlyLogs = Join-Path $directory 'renderer-only logs'
        New-Item -ItemType Directory $rendererOnlyLogs | Out-Null
        $env:NGEN_PROBE_FAIL_AT = 'scene'
        Invoke-NgenRenderer -BuildDirectory $renderBuild -LogDirectory $rendererOnlyLogs -Configuration Debug -Checks { }
        if (Test-Path (Join-Path $rendererOnlyLogs 'Debug-scene.txt')) { throw 'Renderer-only mode unexpectedly ran checked-scene probes' }
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
