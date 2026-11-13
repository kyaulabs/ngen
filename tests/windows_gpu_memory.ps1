#Requires -Version 7.2
$ErrorActionPreference = 'Stop'
$source = Join-Path $PSScriptRoot '../tools/build-windows.ps1'
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($source, [ref]$tokens, [ref]$errors)
if ($errors.Count) { throw ($errors | Out-String) }
foreach ($name in @('Get-NgenGpuMemoryDiagnostic', 'Invoke-NgenPdhGpuMemoryDiagnostic')) {
    $definition = $ast.Find({ param($node)
        $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name
    }, $true)
    if (-not $definition) { throw "GPU diagnostic public boundary not found: $name" }
    . ([scriptblock]::Create($definition.Extent.Text))
}

# Only local process/CIM APIs are substituted. No Windows provider behavior is claimed.
$script:calls = [Collections.Generic.List[object]]::new()
$script:mode = ''
$script:processName = 'flight_demo'
$script:processReads = 0
function Get-CimInstance {
    param([string]$Query, [int]$OperationTimeoutSec, [string]$ErrorAction)
    $script:calls.Add([pscustomobject]@{ Query = $Query; Timeout = $OperationTimeoutSec })
    if ($script:orderFile) { [IO.File]::AppendAllText($script:orderFile, "cim`n") }
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
if (-not $IsWindows) {
    # A real child at the executable boundary, not a Windows PDH simulation claim.
    $fixtureRoot = Join-Path ([IO.Path]::GetTempPath()) ('ngen pdh child ' + [guid]::NewGuid())
    New-Item -ItemType Directory -Path $fixtureRoot | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $fixtureRoot 'System32') | Out-Null
    $counterTool = Join-Path $fixtureRoot 'System32/typeperf.exe'
    try {
        [IO.File]::WriteAllText($counterTool, @'
#!/usr/bin/env python3
import sys
from pathlib import Path
assert sys.argv[1:7] == ['-sc', '1', '-si', '1', '-f', 'CSV']
tail=sys.argv[7:]
output=None
if tail[:1] == ['-o']:
    output=Path(tail[1]);tail=tail[2:]
    assert not output.exists()
assert tail == [r'\GPU Process Memory(pid_42_*)' + chr(92) + name for name in ('Dedicated Usage', 'Shared Usage', 'Total Committed')]
if output:
    output.write_text('"PDH-CSV 4.0","counter fixture"\n"timestamp","128.000000"\n',encoding='utf-16')
print('The command completed successfully.')
if output:
    print('csv_path='+str(output))
'@)
        & chmod +x $counterTool
        if ($LASTEXITCODE) { throw 'Could not prepare the owned executable fixture.' }
        $pdh = Invoke-NgenPdhGpuMemoryDiagnostic -FilePath $counterTool -ProcessId 42
        if (-not $pdh.diagnostic_only -or $pdh.status -cne 'completed' -or $pdh.exit_code -ne 0 -or
            $pdh.stdout -notmatch 'command completed successfully' -or $pdh.csv -notmatch '128.000000' -or
            $pdh.csv_status -cne 'captured' -or $pdh.csv_bytes -le 0 -or $pdh.stderr -or $pdh.timeout_ms -ne 2000) {
            throw 'Successful typeperf status text did not include the required captured CSV values.'
        }
        if ($pdh.stdout -notmatch 'csv_path=([^\r\n]+)') { throw 'CSV cleanup fixture did not report its owned path.' }
        $capturedPath = $Matches[1]
        if ((Test-Path -LiteralPath $capturedPath) -or (Test-Path -LiteralPath (Split-Path -Parent $capturedPath))) {
            throw 'The PDH helper left its private counter file or directory behind.'
        }
        [IO.File]::WriteAllText($counterTool, "#!/usr/bin/env python3`nprint('The command completed successfully.')`n")
        $pdh = Invoke-NgenPdhGpuMemoryDiagnostic -FilePath $counterTool -ProcessId 42
        if ($pdh.status -cne 'missing_or_invalid_csv' -or $pdh.csv -or $pdh.csv_status -cne 'missing') {
            throw 'A zero exit and status message were accepted without captured counter data.'
        }
        [IO.File]::WriteAllText($counterTool, "#!/usr/bin/env python3`nimport sys`nfrom pathlib import Path`nPath(sys.argv[sys.argv.index('-o')+1]).write_bytes(b'x'*131073)`n")
        $pdh = Invoke-NgenPdhGpuMemoryDiagnostic -FilePath $counterTool -ProcessId 42
        if ($pdh.status -cne 'output_limit' -or $pdh.csv -or $pdh.csv_status -cne 'output_limit') {
            throw 'An oversized counter file was read or accepted.'
        }
        [IO.File]::WriteAllText($counterTool, "#!/usr/bin/env python3`nimport os,time`nprint('helper_pid='+str(os.getpid()),flush=True)`ntime.sleep(10)`n")
        $pdh = Invoke-NgenPdhGpuMemoryDiagnostic -FilePath $counterTool -ProcessId 42
        if ($pdh.status -cne 'timed_out' -or $pdh.elapsed_ms -lt 1900 -or $pdh.elapsed_ms -gt 5000 -or $pdh.stdout -notmatch 'helper_pid=(\d+)') {
            throw 'The PDH helper deadline did not stop its owned slow child.'
        }
        $helperId = [int]$Matches[1]
        $remainingChild = Microsoft.PowerShell.Management\Get-Process -Id $helperId -ErrorAction SilentlyContinue
        if ($remainingChild) { throw 'The timed-out PDH helper escaped its owner.' }
        [IO.File]::WriteAllText($counterTool, "#!/usr/bin/env python3`nprint('x'*70000)`n")
        $pdh = Invoke-NgenPdhGpuMemoryDiagnostic -FilePath $counterTool -ProcessId 42
        if ($pdh.status -cne 'output_limit' -or $pdh.stdout.Length -ne 65536) {
            throw 'PDH output was unbounded or an oversized result was accepted.'
        }
        [IO.File]::WriteAllText($counterTool, "#!/usr/bin/env python3`nimport sys`nprint('x'*5000,file=sys.stderr)`n")
        $pdh = Invoke-NgenPdhGpuMemoryDiagnostic -FilePath $counterTool -ProcessId 42
        if ($pdh.status -cne 'output_limit' -or $pdh.stderr.Length -ne 4096) {
            throw 'PDH stderr exceeded its bound or an oversized diagnostic was accepted.'
        }
        [IO.File]::WriteAllText($counterTool, "#!/usr/bin/env python3`nimport sys`nprint('provider error',file=sys.stderr)`nsys.exit(7)`n")
        $pdh = Invoke-NgenPdhGpuMemoryDiagnostic -FilePath $counterTool -ProcessId 42
        if ($pdh.status -cne 'error' -or $pdh.exit_code -ne 7 -or $pdh.stderr -notmatch 'provider error') {
            throw 'A failed direct PDH command was reported as successful.'
        }
        $pdh = Invoke-NgenPdhGpuMemoryDiagnostic -FilePath (Join-Path $fixtureRoot 'missing.exe') -ProcessId 42
        if ($pdh.status -cne 'error' -or $pdh.stdout -or $pdh.exit_code -ne $null) {
            throw 'An unavailable PDH executable was reported as a successful reference.'
        }
        $oldSystemRoot = [Environment]::GetEnvironmentVariable('SystemRoot', 'Process')
        try {
            $env:SystemRoot = $fixtureRoot
            $script:orderFile = Join-Path $fixtureRoot 'order.txt'
            $orderLiteral = $script:orderFile | ConvertTo-Json -Compress
            [IO.File]::WriteAllText($counterTool, "#!/usr/bin/env python3`nimport sys`nfrom pathlib import Path`nwith Path($orderLiteral).open('a') as out: out.write('pdh\n')`nPath(sys.argv[sys.argv.index('-o')+1]).write_text('PDH-CSV 4.0,counter\ntimestamp,128\n',encoding='utf-16')`nprint('counter reference')`n")
            $script:mode = ''; $script:processReads = 0; $script:calls.Clear()
            $result = Get-NgenGpuMemoryDiagnostic -ProcessId 42 -ComparePdh
            $order = Get-Content -LiteralPath $script:orderFile
            if (($order -join ',') -cne 'pdh,cim,cim,cim,cim,pdh' -or $result.schema -ne 2 -or
                $result.pdh_before.status -cne 'completed' -or $result.pdh_after.status -cne 'completed' -or
                $result.queries.Count -ne 4 -or $result.target_status -cne 'same_process') {
                throw 'The direct PDH references did not bracket the unchanged CIM comparison.'
            }
            $script:mode = 'reused'; $script:processReads = 0; $script:calls.Clear()
            $result = Get-NgenGpuMemoryDiagnostic -ProcessId 42 -ComparePdh
            if ($result.target_status -cne 'unavailable_or_changed' -or $result.queries.Count -or
                $result.pdh_before.status -cne 'target_unavailable_or_changed' -or
                $result.pdh_before.stdout -or $result.pdh_before.stderr -or $result.pdh_before.csv -or
                $result.pdh_before.csv_bytes -ne 0 -or $result.pdh_after -or $script:calls.Count) {
                throw 'The PDH comparison retained output or continued after target identity changed.'
            }
        } finally {
            $script:orderFile = $null
            if ($null -eq $oldSystemRoot) {
                [Environment]::SetEnvironmentVariable('SystemRoot', [NullString]::Value, 'Process')
            } else {
                [Environment]::SetEnvironmentVariable('SystemRoot', $oldSystemRoot, 'Process')
            }
        }
    } finally {
        Remove-Item -LiteralPath $fixtureRoot -Recurse -Force
    }
}
Write-Host 'GPU provider comparison boundaries passed; native Windows confirmation remains required.'
# Expected rejected CLI children must not become the Actions shell's final exit code.
exit 0
