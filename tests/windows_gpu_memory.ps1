#Requires -Version 7.2
$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot '../tools/build-windows.ps1'
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($source, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw ($errors | Out-String) }
$definition = $ast.Find({ param($node)
    $node -is [Management.Automation.Language.FunctionDefinitionAst] -and
        $node.Name -eq 'Get-NgenGpuMemoryDiagnostic'
}, $true)
if (-not $definition) { throw 'GPU diagnostic public boundary not found.' }
. ([scriptblock]::Create($definition.Extent.Text))

# Only local process/CIM APIs are substituted. No Windows provider behavior is claimed.
$script:calls = [Collections.Generic.List[object]]::new()
$script:mode = ''
$script:processName = 'flight_demo'
$script:processReads = 0
function Get-CimInstance {
    param([string]$Query, [int]$OperationTimeoutSec, [string]$ErrorAction)
    $script:calls.Add([pscustomobject]@{ Query = $Query; Timeout = $OperationTimeoutSec })
    if ($script:mode -eq 'first-error' -and $script:calls.Count -eq 1) {
        $record = [Management.Automation.ErrorRecord]::new(
            [Microsoft.Management.Infrastructure.CimException]::new('fixture provider failure'),
            ('bounded-id-' + ('x' * 600)), [Management.Automation.ErrorCategory]::OperationTimeout, $null)
        Write-Error -ErrorRecord $record -ErrorAction Stop
    }
    $row = [pscustomobject]@{ Name = 'pid_42_luid_0x00000000_0x1234_phys_0'; DedicatedUsage = 128L
        SharedUsage = 16L; TotalCommitted = 160L; Unrelated = 'unrelated fields must stay private' }
    if ($script:mode -eq 'foreign') { $row.Name = 'pid_43_luid_0x0_0x1234_phys_0' }
    if ($script:mode -eq 'duplicate') { return $row, $row }
    if ($script:mode -eq 'neighbour') {
        $other = [pscustomobject]@{ Name = 'pid_420_luid_0x0_0x1234_phys_0'; DedicatedUsage = 999L
            SharedUsage = 999L; TotalCommitted = 999L }
        return $other, $row
    }
    if ($script:mode -eq 'capacity') {
        for ($i = 0; $i -lt 65; ++$i) {
            [pscustomobject]@{ Name = "pid_42_luid_0x0_0x1234_phys_$i"; DedicatedUsage = 1L
                SharedUsage = 1L; TotalCommitted = 1L }
        }
        return
    }
    if ($script:mode -eq 'negative') { $row.DedicatedUsage = -1L }
    if ($script:mode -eq 'string') { $row.DedicatedUsage = '128' }
    if ($script:mode -eq 'overflow') { $row.DedicatedUsage = [long]::MaxValue }
    if ($script:mode -eq 'empty') { return }
    if ($script:mode -eq 'zero') { $row.DedicatedUsage = 0L; $row.SharedUsage = 0L; $row.TotalCommitted = 0L }
    return $row
}
function Get-Process {
    param([int]$Id, [string]$ErrorAction)
    ++$script:processReads
    $start = [datetime]'2026-11-13T00:00:00Z'
    if ($script:mode -eq 'reused' -and $script:processReads -ge 3) { $start = $start.AddDays(1) }
    $process = [pscustomobject]@{ Id = 42; ProcessName = $script:processName; HasExited = $false
        StartTime = $start }
    $process | Add-Member -MemberType ScriptMethod -Name Dispose -Value { }
    return $process
}
$result = Get-NgenGpuMemoryDiagnostic -ProcessId 42
$formatted = 'Win32_PerfFormattedData_GPUPerformanceCounters_GPUProcessMemory'
$raw = 'Win32_PerfRawData_GPUPerformanceCounters_GPUProcessMemory'
if (-not $result.diagnostic_only -or $result.process_id -ne 42 -or $script:calls.Count -ne 4 -or
    $result.queries.Count -ne 4) { throw 'Diagnostic did not run the bounded raw/formatted comparison.' }
$expected = @($formatted, $raw, $formatted, $raw)
for ($i = 0; $i -lt 4; ++$i) {
    $query = "SELECT Name,DedicatedUsage,SharedUsage,TotalCommitted FROM $($expected[$i]) WHERE Name LIKE 'pid_42_%'"
    if ($script:calls[$i].Query -cne $query -or $script:calls[$i].Timeout -ne 2 -or
        $result.queries[$i].provider -cne $expected[$i] -or $result.queries[$i].status -cne 'observed') {
        throw 'Provider order, PID-only filter, strict timeout or observed status changed.'
    }
}
if ($result.queries[0].rows[0].dedicated_bytes -ne 128 -or
    $result.queries[0].rows[0].shared_bytes -ne 16 -or
    $result.queries[0].rows[0].total_committed_bytes -ne 160 -or
    ($result | ConvertTo-Json -Depth 8).Contains('unrelated fields must stay private')) {
    throw 'Diagnostic did not project only the bounded owned GPU counter fields.'
}
$script:calls.Clear(); $script:mode = 'first-error'
$result = Get-NgenGpuMemoryDiagnostic -ProcessId 42
if ($result.queries.Count -ne 4 -or $result.queries[0].status -cne 'error' -or
    $result.queries[1].status -cne 'observed' -or
    $result.queries[0].error_details.fully_qualified_error_id.Length -ne 512 -or
    $result.queries[0].error_details.native_error_code -cne 'Ok' -or
    $result.queries[0].error_details.status_code -ne 0) {
    throw 'Provider failure lost the remaining comparison or bounded typed CIM diagnostics.'
}
# WQL '_' is a wildcard: the unchanged formal query can also return PID-prefix neighbours.
$script:calls.Clear(); $script:mode = 'neighbour'
$result = Get-NgenGpuMemoryDiagnostic -ProcessId 42
if ($result.queries.Count -ne 4 -or @($result.queries | Where-Object {
    $_.status -cne 'observed' -or $_.rows.Count -ne 1 -or $_.rows[0].dedicated_bytes -ne 128
}).Count) { throw 'A valid own observation was lost to a WQL PID-prefix neighbour.' }
$script:calls.Clear(); $script:mode = 'foreign'
$result = Get-NgenGpuMemoryDiagnostic -ProcessId 42
if ($result.queries.Count -ne 4 -or @($result.queries | Where-Object { $_.status -cne 'no_own_instances' -or $_.rows.Count }).Count) {
    throw 'A diagnostic accepted or disclosed another process GPU instance.'
}
foreach ($case in @('duplicate', 'capacity', 'negative', 'string', 'overflow')) {
    $script:calls.Clear(); $script:mode = $case
    $result = Get-NgenGpuMemoryDiagnostic -ProcessId 42
    if ($result.queries.Count -ne 4 -or @($result.queries | Where-Object { $_.status -cne 'error' -or $_.rows.Count }).Count) {
        throw "Invalid GPU diagnostic rows were accepted: $case"
    }
}
foreach ($case in @('zero', 'empty')) {
    $script:calls.Clear(); $script:mode = $case
    $result = Get-NgenGpuMemoryDiagnostic -ProcessId 42
    $expectedStatus = if ($case -eq 'zero') { 'zero' } else { 'no_own_instances' }
    if ($result.queries.Count -ne 4 -or @($result.queries | Where-Object { $_.status -cne $expectedStatus }).Count) {
        throw "Missing or zero GPU counters were labelled observed: $case"
    }
}
$script:calls.Clear(); $script:mode = ''; $script:processName = 'unrelated'
$rejected = $false
try { Get-NgenGpuMemoryDiagnostic -ProcessId 42 | Out-Null } catch { $rejected = $true }
if (-not $rejected -or $script:calls.Count) { throw 'A non-flight target reached the GPU provider.' }
$script:processName = 'flight_demo'
$script:calls.Clear(); $script:mode = 'reused'; $script:processReads = 0
$result = Get-NgenGpuMemoryDiagnostic -ProcessId 42
if ($script:calls.Count -ne 1 -or $result.queries.Count -ne 1 -or
    $result.target_status -cne 'unavailable_or_changed' -or
    $result.queries[0].status -cne 'target_unavailable_or_changed' -or $result.queries[0].rows.Count) {
    throw 'PID reuse did not stop diagnosis and discard the uncorrelated rows.'
}
if (-not $IsWindows) {
    $shell = (Microsoft.PowerShell.Management\Get-Process -Id $PID).Path
    $temporary = Join-Path ([IO.Path]::GetTempPath()) ('ngen gpu probe ' + [guid]::NewGuid())
    try {
        & $shell -NoProfile -File $source -DiagnoseGpuMemoryProcessId 42 *> $temporary
        if ($LASTEXITCODE -ne 2 -or (Get-Content -LiteralPath $temporary -Raw) -notmatch 'requires Windows') {
            throw 'Diagnostic-only CLI did not reject the unsupported host before preparing a build.'
        }
        foreach ($extra in @(@('-Compiler', 'MSVC'), @('-GpuInfo'), @('-ScenePackage', 'unused.ngas'))) {
            & $shell -NoProfile -File $source -DiagnoseGpuMemoryProcessId 42 @extra *> $temporary
            if ($LASTEXITCODE -eq 0 -or $LASTEXITCODE -eq 2) { throw 'Diagnostic mode accepted a mixed build/capture operation.' }
        }
        & $shell -NoProfile -File $source -DiagnoseGpuMemoryProcessId 0 *> $temporary
        if ($LASTEXITCODE -eq 0 -or $LASTEXITCODE -eq 2) { throw 'Diagnostic mode accepted an invalid process ID.' }
    } finally {
        Remove-Item -LiteralPath $temporary -ErrorAction SilentlyContinue
    }
}
Write-Host 'GPU provider comparison boundaries passed; native Windows confirmation remains required.'
# Expected rejected CLI children must not become the Actions shell's final exit code.
exit 0
