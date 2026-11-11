#Requires -Version 7.2
$ErrorActionPreference = 'Stop'
$scriptPath = Join-Path $PSScriptRoot '../tools/build-windows.ps1'
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($scriptPath, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw ($errors | Out-String) }
foreach ($name in @('Invoke-NgenFlight', 'Get-NgenSceneIdentity')) {
    $definition = $ast.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
    }, $true)
    if (-not $definition) { throw "Missing public boundary $name" }
    . ([scriptblock]::Create($definition.Extent.Text))
}
# Native process/memory calls are the system boundaries here. C++/SDL behavior
# and real Windows execution require their separate native gates.
$script:calls = [Collections.Generic.List[object]]::new()
$script:failure = ''
function Save-TestOutputs {
    param([string[]]$Arguments)
    foreach ($argument in $Arguments) {
        if ($argument.StartsWith('--capture=') -and $script:failure -ne 'missing-capture') { [IO.File]::WriteAllText($argument.Substring(10), 'private capture fixture') }
        if ($argument.StartsWith('--report=')) {
            $rows = [Collections.Generic.List[string]]::new()
            $rows.Add('frame,tick,frame_ns,host_ns,gpu_ready')
            for ($i = 0; $i -lt 330; ++$i) { $rows.Add("$i,$($i+1),1,$($i+1000),1") }
            [IO.File]::WriteAllLines($argument.Substring(9), $rows)
        }
    }
}
function Invoke-NgenNative {
    param([string]$FilePath, [string[]]$Arguments, [string]$LogPath)
    if ('--output' -in $Arguments) {
        $destination = $Arguments[[array]::IndexOf($Arguments, '--output') + 1]
        $bundle = Join-Path $destination 'ngen-flight'
        New-Item -ItemType Directory -Path (Join-Path $bundle 'assets') | Out-Null
        [IO.File]::WriteAllText((Join-Path $bundle 'flight_demo.exe'), 'local executable fixture')
        Copy-Item -LiteralPath $script:scene -Destination (Join-Path $bundle 'assets/ship.ngas')
    } elseif ($FilePath.Contains('ngen-flight')) {
        if ($script:failure -eq 'offline-failure') { throw 'fixture offline process failed' }
        if ($env:NGEN_FLIGHT_ENV -cne 'before' -or $env:VULKAN_SDK -or $env:VK_LAYER_PATH -or
            $env:VK_INSTANCE_LAYERS -or $env:PATH.Contains('fixture-sdk')) { throw 'Offline startup retained the SDK runtime environment' }
    } elseif ($env:NGEN_FLIGHT_ENV -cne 'active') { throw 'Runtime environment is not active' }
    $script:calls.Add([pscustomobject]@{ Kind = 'native'; Path = $FilePath; Arguments = $Arguments })
    [IO.File]::WriteAllText($LogPath, "flight_verified=1`n")
    Save-TestOutputs -Arguments $Arguments
}
function Invoke-NgenWorkload {
    param([string]$FilePath, [string[]]$Arguments, [string]$LogPath, [string]$MemoryPath,
        [switch]$GpuMemory, [string]$FrameReport, [int]$TimeoutMilliseconds = 180000)
    if ($env:NGEN_FLIGHT_ENV -cne 'active') { throw 'Runtime environment is not active' }
    $script:calls.Add([pscustomobject]@{ Kind = 'workload'; Path = $FilePath; Arguments = $Arguments; GpuMemory = [bool]$GpuMemory; FrameReport = $FrameReport })
    if ($script:failure -eq 'process-failure') { throw 'fixture native process failed' }
    if ($script:failure -eq 'mutate-package') { [IO.File]::WriteAllText($script:scene, 'mutated fixture') }
    [IO.File]::WriteAllText($LogPath, "flight_frames=330 audio_pending=0 gpu_timing_rows=330`n")
    [IO.File]::WriteAllText($MemoryPath, '{"exit_code":0,"timed_out":false,"observed_peak_working_set_bytes":1048576}')
    Save-TestOutputs -Arguments $Arguments
}
$temporary = Join-Path ([IO.Path]::GetTempPath()) ('ngen flight handoff ' + [guid]::NewGuid())
New-Item -ItemType Directory -Path $temporary | Out-Null
$old = $env:NGEN_FLIGHT_ENV
$oldSystemRoot = $env:SystemRoot
$oldPath = $env:PATH
$oldSdk = $env:VULKAN_SDK
try {
    $build = Join-Path $temporary 'build'; $logs = Join-Path $temporary 'logs'
    New-Item -ItemType Directory -Path $build, $logs | Out-Null
    Set-Content -LiteralPath (Join-Path $build 'conanrun.ps1') '$env:NGEN_FLIGHT_ENV = "active"'
    Set-Content -LiteralPath (Join-Path $build 'deactivate_conanrun.ps1') '$env:NGEN_FLIGHT_ENV = "before"'
    $script:scene = Join-Path $temporary 'accepted ship with spaces.ngas'
    [IO.File]::WriteAllText($scene, 'local cooked scene fixture')
    $env:NGEN_FLIGHT_ENV = 'before'
    $parameters = @{ BuildDirectory = $build; LogDirectory = $logs; Configuration = 'Debug'
        ScenePackage = $scene; Python = 'python'; SourceRoot = (Split-Path -Parent $PSScriptRoot) }
    Invoke-NgenFlight @parameters
    if ($script:calls.Count -ne 2 -or $script:calls[0].Arguments[0] -cne $scene -or
        '--verify' -notin $script:calls[0].Arguments -or '--walkthrough' -notin $script:calls[1].Arguments -or
        '--frames=330' -notin $script:calls[1].Arguments -or '--validation' -notin $script:calls[1].Arguments) {
        throw 'Flight headless admission or real-app correctness invocation was lost'
    }
    if ($env:NGEN_FLIGHT_ENV -cne 'before') { throw 'Flight runtime environment was not restored' }
    if (-not (Test-Path -LiteralPath (Join-Path $logs 'Debug-flight.artifacts.json'))) {
        throw 'Flight capture/report identities were not retained'
    }
    foreach ($case in @('missing-capture', 'process-failure', 'mutate-package', 'owner-capture')) {
        $caseLogs = Join-Path $temporary $case
        New-Item -ItemType Directory -Path $caseLogs | Out-Null
        $candidate = $parameters.Clone(); $candidate.LogDirectory = $caseLogs
        $owner = Join-Path $caseLogs 'Debug-flight.bmp'
        if ($case -eq 'owner-capture') { [IO.File]::WriteAllText($owner, 'owner capture must survive') }
        $script:failure = $case; $count = $script:calls.Count; $failed = $false
        try { Invoke-NgenFlight @candidate } catch { $failed = $true }
        if (-not $failed -or $env:NGEN_FLIGHT_ENV -cne 'before' -or
            (Test-Path -LiteralPath (Join-Path $caseLogs 'Debug-flight.artifacts.json'))) { throw 'Flight failure was accepted or runtime cleanup was lost' }
        if ($case -eq 'owner-capture' -and ([IO.File]::ReadAllText($owner) -cne 'owner capture must survive' -or
            $script:calls.Count -ne $count)) { throw 'Flight launched before freshness admission or overwrote owner bytes' }
        [IO.File]::WriteAllText($scene, 'local cooked scene fixture')
    }
    $script:failure = ''; $script:calls.Clear()
    $env:SystemRoot = Join-Path $temporary 'Windows'
    $env:PATH = 'fixture-sdk' + [IO.Path]::PathSeparator + $oldPath
    $env:VULKAN_SDK = 'fixture-sdk'
    $beforePath = $env:PATH
    $parameters.Configuration = 'Release'
    Invoke-NgenFlight @parameters
    $clean = @($script:calls | Where-Object { $_.Kind -eq 'native' -and '--replay' -in $_.Arguments -and -not $_.Path.Contains('ngen-flight') })
    $memory = @($script:calls | Where-Object { $_.Kind -eq 'workload' -and '--replay' -in $_.Arguments })
    if ($clean.Count -ne 1 -or $memory.Count -ne 1 -or -not $memory[0].GpuMemory -or
        -not $memory[0].FrameReport -or '--validation' -in $clean[0].Arguments -or
        '--validation' -in $memory[0].Arguments) { throw 'Clean replay timing and scoped instrumented GPU memory were not kept separate' }
    foreach ($required in @('--frames=2400', '--warmup=60', '--width=1920', '--height=1080')) {
        if ($required -notin $clean[0].Arguments -or $required -notin $memory[0].Arguments) {
            throw 'RTX replay and memory observation do not share the declared full-quality scenario'
        }
    }
    $bundle = Join-Path $temporary 'offline/ngen-flight'
    if (-not (Test-Path -LiteralPath (Join-Path $bundle 'flight_demo.exe')) -or
        $script:calls[-1].Path -cne (Join-Path $bundle 'flight_demo.exe') -or
        $script:calls[-1].Arguments[0] -cne (Join-Path $bundle 'assets/ship.ngas') -or
        '--replay' -notin $script:calls[-1].Arguments -or '--frames=3' -notin $script:calls[-1].Arguments) {
        throw 'Release did not create and start the app-local offline bundle'
    }
    if ($env:PATH -cne $beforePath -or $env:VULKAN_SDK -cne 'fixture-sdk' -or
        $env:NGEN_FLIGHT_ENV -cne 'before') { throw 'Offline verification did not restore the owner environment' }
    $failureRoot = Join-Path $temporary 'offline failure'
    $failureBuild = Join-Path $failureRoot 'Release'; $failureLogs = Join-Path $failureRoot 'logs'
    New-Item -ItemType Directory -Path $failureBuild, $failureLogs | Out-Null
    Copy-Item -LiteralPath (Join-Path $build 'conanrun.ps1'), (Join-Path $build 'deactivate_conanrun.ps1') -Destination $failureBuild
    $parameters.BuildDirectory = $failureBuild; $parameters.LogDirectory = $failureLogs
    $script:failure = 'offline-failure'; $failed = $false; $location = (Get-Location).Path
    try { Invoke-NgenFlight @parameters } catch { $failed = $_.Exception.Message -ceq 'fixture offline process failed' }
    if (-not $failed -or $env:PATH -cne $beforePath -or $env:VULKAN_SDK -cne 'fixture-sdk' -or
        $env:NGEN_FLIGHT_ENV -cne 'before' -or (Get-Location).Path -cne $location) { throw 'Offline failure lost owner environment or location cleanup' }
    Write-Host 'PASS: flight orchestration/process boundaries; not native Windows verification'
} finally {
    $env:NGEN_FLIGHT_ENV = $old
    $env:SystemRoot = $oldSystemRoot
    $env:PATH = $oldPath
    $env:VULKAN_SDK = $oldSdk
    Remove-Item -LiteralPath $temporary -Recurse -Force
}
exit 0
