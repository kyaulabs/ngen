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
    [Parameter(ParameterSetName = 'CPU')]
    [switch]$Scene,
    [Parameter(ParameterSetName = 'CPU')]
    [switch]$Inspector,
    [Parameter(ParameterSetName = 'CPU')]
    [switch]$FlightDemo,
    [Parameter(ParameterSetName = 'CPU')]
    [Parameter(Mandatory, ParameterSetName = 'Measurement')]
    [string]$ScenePackage = '',
    [Parameter(Mandatory, ParameterSetName = 'Measurement')]
    [string]$MeasureWorkload,
    [Parameter(Mandatory, ParameterSetName = 'GPU')]
    [switch]$GpuInfo,
    [Parameter(Mandatory, ParameterSetName = 'GpuMemoryDiagnostic')]
    [ValidateRange(1, 2147483647)]
    [int]$DiagnoseGpuMemoryProcessId
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

function Get-NgenSceneIdentity {
    param([Parameter(Mandatory)][string]$Path)
    $item = Get-Item -LiteralPath $Path -Force -ErrorAction Stop
    if ($item -isnot [IO.FileInfo] -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
        $item.Extension -ine '.ngas' -or $item.Length -lt 1 -or $item.Length -gt 268435456) {
        throw 'ScenePackage must be a nonempty regular .ngas file of at most 256 MiB, not a directory or link.'
    }
    return [ordered]@{
        schema = 1
        name = $item.Name
        bytes = $item.Length
        sha256 = (Get-FileHash -LiteralPath $item.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
    }
}

function Invoke-NgenPdhGpuMemoryDiagnostic {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter(Mandatory)][ValidateRange(1, 2147483647)][int]$ProcessId
    )
    $result = [ordered]@{ diagnostic_only = $true; source = 'typeperf/PDH'; process_id = $ProcessId
        status = 'not_run'; timeout_ms = 2000; elapsed_ms = 0L; exit_code = $null
        stdout = ''; stderr = ''; error_type = ''; error_hresult = $null }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo.FileName = $FilePath
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.CreateNoWindow = $true
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    foreach ($argument in @('-sc', '1', '-si', '1', '-f', 'CSV')) {
        $process.StartInfo.ArgumentList.Add($argument)
    }
    foreach ($counter in @('Dedicated Usage', 'Shared Usage', 'Total Committed')) {
        $process.StartInfo.ArgumentList.Add("\GPU Process Memory(pid_$($ProcessId)_*)\$counter")
    }
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $started = $false
    try {
        $started = $process.Start()
        if (-not $started) { throw 'Could not start the PDH reference tool.' }
        $outBuffer = [char[]]::new(65537); $errorBuffer = [char[]]::new(4097)
        $output = $process.StandardOutput.ReadBlockAsync($outBuffer, 0, $outBuffer.Length)
        $errors = $process.StandardError.ReadBlockAsync($errorBuffer, 0, $errorBuffer.Length)
        $remaining = [int][Math]::Max(0, 2000 - $clock.ElapsedMilliseconds)
        $finished = $process.WaitForExit($remaining)
        if (-not $finished) {
            try { $process.Kill($true) } catch [InvalidOperationException] { }
        }
        $process.WaitForExit()
        $count = $output.GetAwaiter().GetResult()
        $errorCount = $errors.GetAwaiter().GetResult()
        $result.exit_code = $process.ExitCode
        $result.elapsed_ms = $clock.ElapsedMilliseconds
        $result.stdout = [string]::new($outBuffer, 0, [Math]::Min($count, 65536))
        $result.stderr = [string]::new($errorBuffer, 0, [Math]::Min($errorCount, 4096))
        $result.status = if ($count -gt 65536 -or $errorCount -gt 4096) { 'output_limit' }
            elseif (-not $finished -or $result.elapsed_ms -gt 2000) { 'timed_out' }
            elseif ($process.ExitCode -ne 0 -or $errorCount) { 'error' }
            else { 'completed' }
    } catch {
        $result.status = 'error'
        $result.error_type = $_.Exception.GetType().Name
        $result.error_hresult = $_.Exception.HResult
        $result.elapsed_ms = $clock.ElapsedMilliseconds
    } finally {
        if ($started -and -not $process.HasExited) {
            try { $process.Kill($true) } catch [InvalidOperationException] { }
            $process.WaitForExit()
        }
        $process.Dispose()
    }
    return $result
}

function Get-NgenGpuMemoryDiagnostic {
    param(
        [Parameter(Mandatory)][ValidateRange(1, 2147483647)][int]$ProcessId,
        [switch]$ComparePdh
    )
    $target = Get-Process -Id $ProcessId -ErrorAction Stop
    try {
        if ($target.Id -ne $ProcessId -or $target.HasExited -or $target.ProcessName -ine 'flight_demo') {
            throw 'GPU diagnosis requires the selected live flight_demo process.'
        }
        $startedAt = $target.StartTime.ToUniversalTime()
    } finally {
        $target.Dispose()
    }
    $formatted = 'Win32_PerfFormattedData_GPUPerformanceCounters_GPUProcessMemory'
    $raw = 'Win32_PerfRawData_GPUPerformanceCounters_GPUProcessMemory'
    $sameTarget = {
        try {
            $current = Get-Process -Id $ProcessId -ErrorAction Stop
            try {
                return $current.Id -eq $ProcessId -and -not $current.HasExited -and
                    $current.ProcessName -ieq 'flight_demo' -and
                    $current.StartTime.ToUniversalTime().Ticks -eq $startedAt.Ticks
            } finally {
                $current.Dispose()
            }
        } catch {
            return $false
        }
    }
    $targetStatus = 'same_process'
    $pdhBefore = $null; $pdhAfter = $null
    $readPdh = {
        if (-not (& $sameTarget)) {
            return [ordered]@{ diagnostic_only = $true; status = 'target_unavailable_or_changed'
                target_valid_after = $false; stdout = ''; stderr = '' }
        }
        $tool = Join-Path (Join-Path $env:SystemRoot 'System32') 'typeperf.exe'
        $reference = Invoke-NgenPdhGpuMemoryDiagnostic -FilePath $tool -ProcessId $ProcessId
        $reference.target_valid_after = & $sameTarget
        if (-not $reference.target_valid_after) {
            $reference.status = 'target_unavailable_or_changed'
            $reference.stdout = ''; $reference.stderr = ''
        }
        return $reference
    }
    if ($ComparePdh) {
        $pdhBefore = & $readPdh
        if (-not $pdhBefore.target_valid_after) { $targetStatus = 'unavailable_or_changed' }
    }
    $attempts = [Collections.Generic.List[object]]::new()
    foreach ($provider in @($formatted, $raw, $formatted, $raw)) {
        if ($targetStatus -ne 'same_process' -or -not (& $sameTarget)) { $targetStatus = 'unavailable_or_changed'; break }
        $clock = [Diagnostics.Stopwatch]::StartNew()
        $attempt = [ordered]@{ provider = $provider; status = 'no_own_instances'
            elapsed_ms = 0L; rows = @(); error_details = $null }
        try {
            $rows = @(Get-CimInstance -Query "SELECT Name,DedicatedUsage,SharedUsage,TotalCommitted FROM $provider WHERE Name LIKE 'pid_$($ProcessId)_%'" -OperationTimeoutSec 2 -ErrorAction Stop | Select-Object -First 65)
            if ($rows.Count -gt 64) { throw 'GPU observation capacity exceeded.' }
            $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
            $owned = [Collections.Generic.List[object]]::new()
            foreach ($row in $rows) {
                if ($row.Name -isnot [string] -or $row.Name.Length -gt 128) { throw 'Invalid GPU instance name.' }
                # WQL '_' is a wildcard; match the formal observer's exact PID post-filter.
                if (-not $row.Name.StartsWith("pid_$($ProcessId)_", [StringComparison]::OrdinalIgnoreCase)) { continue }
                if ($row.Name -notmatch "^pid_$($ProcessId)_luid_0x[0-9a-f]+_0x[0-9a-f]+_phys_[0-9]+$") {
                    throw 'Unsupported own-process GPU instance.'
                }
                if (-not $seen.Add($row.Name)) { throw 'Duplicate process GPU instance.' }
                foreach ($value in @($row.DedicatedUsage, $row.SharedUsage, $row.TotalCommitted)) {
                    if (($value -isnot [long] -and $value -isnot [ulong] -and $value -isnot [int] -and $value -isnot [uint32]) -or
                        $value -lt 0 -or $value -gt ([long]::MaxValue / 128)) { throw 'Invalid process GPU counter.' }
                }
                $owned.Add($row)
            }
            $rows = $owned.ToArray()
            if ($rows.Count) {
                $attempt.status = 'zero'
                foreach ($row in $rows) {
                    if ($row.DedicatedUsage -gt 0 -or $row.SharedUsage -gt 0 -or $row.TotalCommitted -gt 0) {
                        $attempt.status = 'observed'
                    }
                }
            }
            $attempt.rows = @($rows | ForEach-Object {
                [ordered]@{ name = [string]$_.Name; dedicated_bytes = $_.DedicatedUsage
                    shared_bytes = $_.SharedUsage; total_committed_bytes = $_.TotalCommitted }
            })
        } catch {
            $attempt.status = 'error'
            $identifier = [string]$_.FullyQualifiedErrorId
            $attempt.error_details = [ordered]@{
                type = $_.Exception.GetType().Name
                hresult = $_.Exception.HResult
                fully_qualified_error_id = $identifier.Substring(0, [Math]::Min($identifier.Length, 512))
                category = [string]$_.CategoryInfo.Category
                native_error_code = $null; status_code = $null
            }
            if ($_.Exception -is [Microsoft.Management.Infrastructure.CimException]) {
                $attempt.error_details.native_error_code = [string]$_.Exception.NativeErrorCode
                $attempt.error_details.status_code = [uint32]$_.Exception.StatusCode
            }
        }
        $attempt.elapsed_ms = $clock.ElapsedMilliseconds
        $attempt.target_valid_after = & $sameTarget
        if (-not $attempt.target_valid_after) {
            $attempt.status = 'target_unavailable_or_changed'
            $attempt.rows = @()
            $targetStatus = 'unavailable_or_changed'
        }
        $attempts.Add($attempt)
        if (-not $attempt.target_valid_after) { break }
    }
    if ($ComparePdh -and $targetStatus -eq 'same_process') {
        $pdhAfter = & $readPdh
        if (-not $pdhAfter.target_valid_after) { $targetStatus = 'unavailable_or_changed' }
    }
    return [ordered]@{ schema = $(if ($ComparePdh) { 2 } else { 1 })
        diagnostic_only = $true; process_id = $ProcessId; target_status = $targetStatus
        process_started_utc = $startedAt.ToString('O')
        operation_timeout_seconds = 2; queries = $attempts.ToArray()
        pdh_before = $pdhBefore; pdh_after = $pdhAfter }
}

function Invoke-NgenWorkload {
    param(
        [Parameter(Mandatory)][string]$FilePath,
        [Parameter(Mandatory)][string[]]$Arguments,
        [Parameter(Mandatory)][string]$LogPath,
        [Parameter(Mandatory)][string]$MemoryPath,
        [switch]$GpuMemory,
        [string]$FrameReport = '',
        [ValidateRange(1000, 600000)][int]$TimeoutMilliseconds = 180000
    )
    $errorPath = "$LogPath.stderr.txt"
    foreach ($path in @($LogPath, $errorPath, $MemoryPath)) {
        if (Test-Path -LiteralPath $path) { throw 'Workload evidence path is not fresh.' }
    }
    $process = [Diagnostics.Process]::new()
    $process.StartInfo.FileName = $FilePath
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    foreach ($argument in $Arguments) { $process.StartInfo.ArgumentList.Add($argument) }
    $clock = [Diagnostics.Stopwatch]::StartNew()
    $clockStarted = [Diagnostics.Stopwatch]::GetTimestamp()
    $peak = 0L
    $resident = 0L
    $timedOut = $false
    $started = $false
    $gpuSamples = [Collections.Generic.List[object]]::new()
    $gpuQueries = [Collections.Generic.List[object]]::new()
    $gpuStatus = 'not_observed'
    $gpuError = ''
    $gpuErrorMessage = ''
    $gpuErrorHresult = $null
    $gpuErrorDetails = $null
    $nextGpuSample = 0L
    try {
        $started = $process.Start()
        if (-not $started) { throw 'Could not start workload process.' }
        $output = $process.StandardOutput.ReadToEndAsync()
        $errors = $process.StandardError.ReadToEndAsync()
        while (-not $process.HasExited) {
            try {
                $process.Refresh()
                $peak = [Math]::Max($peak, $process.PeakWorkingSet64)
                $resident = [Math]::Max($resident, $process.WorkingSet64)
            } catch [InvalidOperationException] { }
            if ($GpuMemory -and $gpuStatus -ne 'unavailable' -and $clock.ElapsedMilliseconds -ge $nextGpuSample) {
                $queryStarted = $clock.ElapsedMilliseconds
                $attempt = [ordered]@{ status = 'no_own_instances'; instances = 0
                    started_timestamp_ticks = [Diagnostics.Stopwatch]::GetTimestamp() }
                try {
                    $rows = @(Get-CimInstance -Query ("SELECT Name,DedicatedUsage,SharedUsage,TotalCommitted FROM Win32_PerfFormattedData_GPUPerformanceCounters_GPUProcessMemory WHERE Name LIKE 'pid_$($process.Id)_%'") -OperationTimeoutSec 2 -ErrorAction Stop)
                    if ($rows.Count -gt 64 -or $gpuQueries.Count -ge 1201) { throw 'GPU observation capacity exceeded.' }
                    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
                    $dedicated = 0L; $shared = 0L; $committed = 0L
                    foreach ($row in $rows) {
                        if ($row.Name -notmatch "^pid_$($process.Id)_") { continue }
                        if ($row.Name -notmatch "^pid_$($process.Id)_luid_0x[0-9a-f]+_0x[0-9a-f]+_phys_[0-9]+$") { throw 'Unsupported own-process GPU instance.' }
                        if (-not $seen.Add($row.Name)) { throw 'Duplicate process GPU instance.' }
                        foreach ($value in @($row.DedicatedUsage, $row.SharedUsage, $row.TotalCommitted)) {
                            if (($value -isnot [long] -and $value -isnot [ulong] -and $value -isnot [int] -and $value -isnot [uint32]) -or
                                $value -lt 0 -or $value -gt ([long]::MaxValue / 128)) { throw 'Invalid process GPU counter.' }
                        }
                        $dedicated += [long]$row.DedicatedUsage
                        $shared += [long]$row.SharedUsage
                        $committed += [long]$row.TotalCommitted
                    }
                    if ($seen.Count) {
                        $attempt.status = if ($dedicated -gt 0 -or $shared -gt 0 -or $committed -gt 0) { 'observed' } else { 'zero' }
                        $attempt.instances = $seen.Count
                        $gpuSamples.Add([ordered]@{
                            timestamp_ticks = [Diagnostics.Stopwatch]::GetTimestamp()
                            elapsed_ms = $clock.ElapsedMilliseconds; query_elapsed_ms = $clock.ElapsedMilliseconds - $queryStarted
                            instances = $seen.Count; dedicated_bytes = $dedicated; shared_bytes = $shared
                            dedicated_plus_shared_bytes = $dedicated + $shared; total_committed_bytes = $committed
                        })
                        $gpuStatus = 'observed'
                    }
                } catch {
                    $attempt.status = 'error'
                    $gpuStatus = 'unavailable'
                    $gpuError = $_.Exception.GetType().Name
                    $detail = [string]$_.Exception.Message
                    $gpuErrorMessage = $detail.Substring(0, [Math]::Min($detail.Length, 512))
                    $gpuErrorHresult = $_.Exception.HResult
                    $identifier = [string]$_.FullyQualifiedErrorId
                    $gpuErrorDetails = [ordered]@{
                        fully_qualified_error_id = $identifier.Substring(0, [Math]::Min($identifier.Length, 512))
                        category = [string]$_.CategoryInfo.Category
                        native_error_code = $null
                        status_code = $null
                    }
                    if ($_.Exception -is [Microsoft.Management.Infrastructure.CimException]) {
                        $gpuErrorDetails.native_error_code = [string]$_.Exception.NativeErrorCode
                        $gpuErrorDetails.status_code = [uint32]$_.Exception.StatusCode
                    }
                } finally {
                    $attempt.timestamp_ticks = [Diagnostics.Stopwatch]::GetTimestamp()
                    $attempt.elapsed_ms = $clock.ElapsedMilliseconds
                    $attempt.query_elapsed_ms = $clock.ElapsedMilliseconds - $queryStarted
                    $attempt.process_exited = $process.HasExited
                    $gpuQueries.Add($attempt)
                }
                $nextGpuSample = $clock.ElapsedMilliseconds + 500
            }
            if ($process.WaitForExit(25)) { break }
            if ($clock.ElapsedMilliseconds -gt $TimeoutMilliseconds) {
                $timedOut = $true
                try { $process.Kill($true) } catch [InvalidOperationException] { }
                break
            }
        }
        $process.WaitForExit()
        # A CIM call may finish after the observation deadline even when the
        # child exits during that call. Preserve its real exit code, but fail timing.
        if ($clock.ElapsedMilliseconds -gt $TimeoutMilliseconds) { $timedOut = $true }
        $stdout = $output.GetAwaiter().GetResult()
        $stderr = $errors.GetAwaiter().GetResult()
        [IO.File]::WriteAllText($LogPath, $stdout)
        [IO.File]::WriteAllText($errorPath, $stderr)
        $observed = [Math]::Max($peak, $resident)
        if ($GpuMemory -and $gpuStatus -eq 'observed' -and
            ($gpuSamples.dedicated_plus_shared_bytes | Measure-Object -Maximum).Maximum -eq 0 -and
            ($gpuSamples.total_committed_bytes | Measure-Object -Maximum).Maximum -eq 0) {
            $gpuStatus = 'not_observed'
            $gpuError = 'EntirelyZeroGpuWorkloadTrace'
        }
        $clockEnded = [Diagnostics.Stopwatch]::GetTimestamp()
        $coverage = $null
        if ($GpuMemory) {
            $windowStart = $clockStarted; $windowEnd = $clockEnded
            $windowScope = 'process_observation'; $windowError = ''; $frameRows = 0
            if ($FrameReport) {
                $windowScope = 'native_measured_frames'
                try {
                    $report = Get-Item -LiteralPath $FrameReport -Force -ErrorAction Stop
                    if ($report -isnot [IO.FileInfo] -or ($report.Attributes -band [IO.FileAttributes]::ReparsePoint) -or
                        $report.Extension -ine '.csv' -or $report.Length -lt 1 -or $report.Length -gt 8388608) { throw 'Invalid native frame report.' }
                    $header = Get-Content -LiteralPath $FrameReport -TotalCount 1
                    if ($header.Length -gt 2048 -or $header.Split(',').Count -gt 64) { throw 'Native frame header exceeds admission.' }
                    # Stop enumeration at the admitted row bound, not after materializing an arbitrary CSV.
                    $frames = @(Import-Csv -LiteralPath $FrameReport | Select-Object -First 3601)
                    if ($frames.Count -lt 1 -or $frames.Count -gt 3600) { throw 'Invalid native frame count.' }
                    $previous = [decimal]-1
                    foreach ($frame in $frames) {
                        if ($frame.host_ns -notmatch '^\d+$' -or $frame.frame_ns -notmatch '^\d+$' -or
                            [decimal]$frame.host_ns -le $previous) { throw 'Invalid native frame clock.' }
                        $previous = [decimal]$frame.host_ns
                    }
                    $windowStart = [long](([decimal]$frames[0].host_ns - [decimal]$frames[0].frame_ns) * [Diagnostics.Stopwatch]::Frequency / 1000000000)
                    $windowEnd = [long]([decimal]$frames[-1].host_ns * [Diagnostics.Stopwatch]::Frequency / 1000000000)
                    if ($windowStart -lt $clockStarted -or $windowEnd -gt $clockEnded -or $windowEnd -lt $windowStart) { throw 'Native and parent clock windows do not correlate.' }
                    $frameRows = $frames.Count
                } catch { $windowError = 'InvalidOrUncorrelatedNativeFrameReport' }
            }
            $validSamples = @($gpuSamples | Where-Object { $_.dedicated_plus_shared_bytes -gt 0 -or $_.total_committed_bytes -gt 0 })
            $missing = @($gpuQueries | Where-Object {
                $_.timestamp_ticks -ge $windowStart -and $_.timestamp_ticks -le $windowEnd -and
                    $_.status -ne 'observed' -and -not $_.process_exited
            })
            $maximumGap = 0L; $previous = $windowStart
            foreach ($sample in $validSamples) {
                if ($sample.timestamp_ticks -ge $windowStart -and $sample.timestamp_ticks -le $windowEnd) {
                    $maximumGap = [Math]::Max($maximumGap, $sample.timestamp_ticks - $previous)
                    $previous = $sample.timestamp_ticks
                }
            }
            $maximumGap = [Math]::Max($maximumGap, $windowEnd - $previous)
            $nearWindow = @($validSamples | Where-Object {
                $_.timestamp_ticks -ge $windowStart - [Diagnostics.Stopwatch]::Frequency -and
                    $_.timestamp_ticks -le $windowEnd + [Diagnostics.Stopwatch]::Frequency
            })
            $complete = -not $windowError -and $nearWindow.Count -gt 0 -and $missing.Count -eq 0 -and
                $maximumGap -le [Diagnostics.Stopwatch]::Frequency
            $coverage = [ordered]@{ complete = $complete; scope = $windowScope; error = $windowError
                start_timestamp_ticks = $windowStart; end_timestamp_ticks = $windowEnd; frame_rows = $frameRows
                missing_queries_in_window = $missing.Count; maximum_allowed_gap_ms = 1000
                maximum_observed_gap_ms = [decimal]$maximumGap * 1000 / [Diagnostics.Stopwatch]::Frequency
                scope_note = 'Every query attempt is retained. Native-frame scope excludes declared startup/exit windows, not bad measured frames. One-second maximum observation gap is not a continuous hard peak guarantee.' }
            if ($gpuStatus -eq 'observed' -and -not $complete) {
                $gpuStatus = 'incomplete'
                $gpuError = 'IncompleteGpuWindowCoverage'
            }
        }
        $gpuBudgetOk = if ($gpuStatus -eq 'observed') {
            ($gpuSamples.dedicated_plus_shared_bytes | Measure-Object -Maximum).Maximum -le 1073741824 -and
                ($gpuSamples.total_committed_bytes | Measure-Object -Maximum).Maximum -le 1073741824
        } else { $null }
        $memory = [ordered]@{
            schema = 1; process_id = $process.Id; exit_code = $process.ExitCode; timed_out = $timedOut
            elapsed_ms = $clock.ElapsedMilliseconds; sample_period_ms = 25
            observed_peak_working_set_bytes = $observed
            observed_peak_counter_bytes = $peak; sampled_resident_bytes = $resident
            scope = 'Observed OS process working set/high-water counter, including startup; not GPU memory or an exact continuous peak.'
        }
        if ($GpuMemory) {
            $memory.schema = 3
            $memory.gpu_memory = [ordered]@{
                status = $gpuStatus; error_type = $gpuError; within_1gib_budget = $gpuBudgetOk
                error_message = $gpuErrorMessage; error_hresult = $gpuErrorHresult
                error_details = $gpuErrorDetails
                provider = 'Win32_PerfFormattedData_GPUPerformanceCounters_GPUProcessMemory'
                target_sample_period_ms = 500; operation_timeout_seconds = 2
                parent_qpc_started_ticks = $clockStarted; parent_qpc_ended_ticks = $clockEnded
                parent_qpc_frequency = [Diagnostics.Stopwatch]::Frequency
                peak_dedicated_bytes = if ($gpuSamples.Count) { ($gpuSamples.dedicated_bytes | Measure-Object -Maximum).Maximum } else { $null }
                peak_shared_bytes = if ($gpuSamples.Count) { ($gpuSamples.shared_bytes | Measure-Object -Maximum).Maximum } else { $null }
                peak_dedicated_plus_shared_bytes = if ($gpuSamples.Count) { ($gpuSamples.dedicated_plus_shared_bytes | Measure-Object -Maximum).Maximum } else { $null }
                peak_total_committed_bytes = if ($gpuSamples.Count) { ($gpuSamples.total_committed_bytes | Measure-Object -Maximum).Maximum } else { $null }
                samples = $gpuSamples.ToArray(); queries = $gpuQueries.ToArray(); coverage = $coverage
                scope = 'Local WDDM per-process adapter counters, summed once per unique instance; dedicated+shared and total committed are separate measures. Sampled/provider-refreshed observations, not an exact continuous peak, global device usage or an uninstrumented timing run. CIM operation timeout is not a hard OS deadline.'
            }
        }
        $memory | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $MemoryPath
        $stdout | Out-Host
        if ($timedOut) { throw 'Workload observation exceeded its deadline.' }
        if ($process.ExitCode -ne 0) { throw "Workload failed with exit code $($process.ExitCode). See $LogPath" }
        if ($stderr.Length) { throw "Unexpected workload stderr. See $errorPath" }
        if ($observed -le 0 -or $observed -gt 6442450944) { throw 'Observed workload working set unavailable or above 6 GiB.' }
        if ($GpuMemory -and $gpuStatus -ne 'observed') { throw 'Process GPU memory observations unavailable or incomplete; inspect retained metadata.' }
        if ($GpuMemory -and -not $gpuBudgetOk) { throw 'Observed process GPU memory exceeds the 1 GiB budget; inspect retained metadata.' }
    } finally {
        if ($started -and -not $process.HasExited) {
            try { $process.Kill($true) } catch [InvalidOperationException] { }
            $process.WaitForExit()
        }
        $process.Dispose()
    }
}

function Invoke-NgenMeasurement {
    param(
        [Parameter(Mandatory)][string]$BuildDirectory,
        [Parameter(Mandatory)][string]$LogDirectory,
        [Parameter(Mandatory)][string]$ScenePackage
    )
    $directory = Get-Item -LiteralPath $BuildDirectory -Force -ErrorAction Stop
    if ($directory -isnot [IO.DirectoryInfo] -or ($directory.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'Measurement requires a regular existing Release build directory, not a link.'
    }
    $BuildDirectory = $directory.FullName
    $cache = Join-Path $BuildDirectory 'CMakeCache.txt'
    $binary = Join-Path $BuildDirectory 'scene_workload.exe'
    $activate = Join-Path $BuildDirectory 'conanrun.ps1'
    $deactivate = Join-Path $BuildDirectory 'deactivate_conanrun.ps1'
    $buildLogs = Join-Path $directory.Parent.FullName 'logs'
    $revisionFile = Join-Path $buildLogs 'revision.txt'
    $treeFile = Join-Path $buildLogs 'working-tree.txt'
    foreach ($path in @($cache, $binary, $activate, $deactivate, $revisionFile, $treeFile)) {
        $item = Get-Item -LiteralPath $path -Force -ErrorAction Stop
        if ($item -isnot [IO.FileInfo] -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $item.Length -gt 67108864) {
            throw 'Measurement build input is missing, linked, nonregular or oversized.'
        }
    }
    $type = [regex]::Matches((Get-Content -Raw -LiteralPath $cache), '(?m)^CMAKE_BUILD_TYPE:STRING=([^\r\n]*)\r?$')
    $revision = (Get-Content -Raw -LiteralPath $revisionFile).Trim()
    if ($type.Count -ne 1 -or $type[0].Groups[1].Value -cne 'Release' -or
        $revision -notmatch '^[0-9a-f]{40}$' -or (Get-Item -LiteralPath $treeFile).Length -ne 0) {
        throw 'Measurement requires a declared clean Release build with its original revision record.'
    }
    $prefix = Join-Path $LogDirectory 'long-stream'
    foreach ($extension in @('txt', 'txt.stderr.txt', 'bmp', 'csv', 'working-set.json', 'artifacts.json', 'input.json')) {
        if (Test-Path -LiteralPath "$prefix.$extension") { throw 'Measurement evidence path is not fresh.' }
    }
    $identity = Get-NgenSceneIdentity -Path $ScenePackage | ConvertTo-Json
    $binaryHash = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()
    [ordered]@{
        schema = 1; declared_build_revision = $revision; build_tree_record_empty = $true
        binary_sha256 = $binaryHash; cmake_cache_sha256 = (Get-FileHash -LiteralPath $cache -Algorithm SHA256).Hash.ToLowerInvariant()
        package = $identity | ConvertFrom-Json
        frames = 3600; warmup = 60; reload_cycles = 4; fixed_extent = '1280x720'
        inspector = $true; exposure = 1; fxaa = $true; bloom = 0.04; validation = $false
        scope = 'Existing Release binary with its original declared build record, no rebuild. Input hashing warms cache. External CIM observation is instrumented memory evidence, not a clean performance run or proof of binary/source equivalence.'
    } | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath "$prefix.input.json"
    try {
        . $activate
        $arguments = @($ScenePackage, '--inspector', '--walkthrough', '--no-resize', '--reload-cycles=4',
            '--frames=3600', '--warmup=60', "--capture=$prefix.bmp", "--report=$prefix.csv")
        Invoke-NgenWorkload -FilePath $binary -Arguments $arguments -LogPath "$prefix.txt" -MemoryPath "$prefix.working-set.json" -GpuMemory -FrameReport "$prefix.csv"
        if ((Get-NgenSceneIdentity -Path $ScenePackage | ConvertTo-Json) -cne $identity -or
            (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant() -cne $binaryHash) {
            throw 'Measurement package or binary changed during observation.'
        }
        $artifacts = foreach ($path in @("$prefix.bmp", "$prefix.csv")) {
            $item = Get-Item -LiteralPath $path -Force -ErrorAction Stop
            if ($item -isnot [IO.FileInfo] -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $item.Length -lt 1 -or $item.Length -gt 8388608) {
                throw 'Measurement capture/report is missing, linked, empty or oversized.'
            }
            [ordered]@{ name = $item.Name; bytes = $item.Length; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
        }
        $artifacts | ConvertTo-Json | Set-Content -LiteralPath "$prefix.artifacts.json"
    } finally {
        . $deactivate
    }
}

function Invoke-NgenFlight {
    param(
        [Parameter(Mandatory)][string]$BuildDirectory,
        [Parameter(Mandatory)][string]$LogDirectory,
        [Parameter(Mandatory)][ValidateSet('Debug', 'Release')][string]$Configuration,
        [Parameter(Mandatory)][string]$ScenePackage,
        [Parameter(Mandatory)][string]$Python,
        [Parameter(Mandatory)][string]$SourceRoot
    )
    $identity = Get-NgenSceneIdentity -Path $ScenePackage | ConvertTo-Json
    $prefix = Join-Path $LogDirectory "$Configuration-flight"
    $bundle = $null
    foreach ($extension in @('verify.txt', 'verify.txt.stderr.txt', 'txt', 'txt.stderr.txt', 'bmp', 'csv', 'working-set.json', 'artifacts.json',
        'replay.txt', 'replay.txt.stderr.txt', 'replay.csv', 'replay.bmp',
        'memory.txt', 'memory.txt.stderr.txt', 'memory.csv', 'memory.bmp', 'memory.working-set.json',
        'package.txt', 'package.txt.stderr.txt', 'offline-verify.txt', 'offline-verify.txt.stderr.txt',
        'offline.txt', 'offline.txt.stderr.txt', 'offline.csv')) {
        if (Test-Path -LiteralPath "$prefix.$extension") { throw 'Flight evidence path is not fresh.' }
    }
    try {
        . (Join-Path $BuildDirectory 'conanrun.ps1')
        $binary = Join-Path $BuildDirectory 'flight_demo.exe'
        Invoke-NgenNative -FilePath $binary -Arguments @($ScenePackage, '--verify') -LogPath "$prefix.verify.txt"
        $arguments = @($ScenePackage, '--walkthrough', '--frames=330', '--warmup=60', '--validation', '--mute',
            "--capture=$prefix.bmp", "--report=$prefix.csv")
        Invoke-NgenWorkload -FilePath $binary -Arguments $arguments -LogPath "$prefix.txt" -MemoryPath "$prefix.working-set.json"
        if ((Get-NgenSceneIdentity -Path $ScenePackage | ConvertTo-Json) -cne $identity) {
            throw 'ScenePackage changed during flight verification.'
        }
        $artifactPaths = @("$prefix.bmp", "$prefix.csv")
        if ($Configuration -eq 'Release') {
            foreach ($mode in @('replay', 'memory')) {
                $report = "$prefix.$mode.csv"; $capture = "$prefix.$mode.bmp"
                $arguments = @($ScenePackage, '--replay', '--frames=2400', '--warmup=60', '--width=1920', '--height=1080',
                    "--capture=$capture", "--report=$report")
                if ($mode -eq 'replay') {
                    # No CIM or per-process observation in the clean timing run.
                    Invoke-NgenNative -FilePath $binary -Arguments $arguments -LogPath "$prefix.$mode.txt"
                } else {
                    Invoke-NgenWorkload -FilePath $binary -Arguments $arguments -LogPath "$prefix.$mode.txt" -MemoryPath "$prefix.$mode.working-set.json" -GpuMemory -FrameReport $report
                }
                $artifactPaths += @($capture, $report)
                if ((Get-NgenSceneIdentity -Path $ScenePackage | ConvertTo-Json) -cne $identity) { throw 'ScenePackage changed during replay.' }
            }
        }
        $artifacts = foreach ($path in $artifactPaths) {
            $item = Get-Item -LiteralPath $path -Force -ErrorAction Stop
            if ($item -isnot [IO.FileInfo] -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $item.Length -lt 1 -or $item.Length -gt 8388608) {
                throw 'Flight capture/report is missing, linked, empty or oversized.'
            }
            [ordered]@{ name = $item.Name; bytes = $item.Length; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
        }
        [ordered]@{ schema = 1; package = $identity | ConvertFrom-Json; files = @($artifacts) } |
            ConvertTo-Json -Depth 5 | Set-Content -LiteralPath "$prefix.artifacts.json"
        if ($Configuration -eq 'Release') {
            $output = Join-Path (Split-Path -Parent $BuildDirectory) 'offline'
            Invoke-NgenNative -FilePath $Python -Arguments @((Join-Path $SourceRoot 'tools/package_demo.py'),
                '--build', $BuildDirectory, '--asset', $ScenePackage, '--output', $output) -LogPath "$prefix.package.txt"
            $bundle = Join-Path $output 'ngen-flight'
        }
    } finally {
        . (Join-Path $BuildDirectory 'deactivate_conanrun.ps1')
    }
    if ($bundle) {
        $saved = @{}
        foreach ($name in @('PATH', 'VULKAN_SDK', 'VK_LAYER_PATH', 'VK_INSTANCE_LAYERS')) {
            $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
        }
        Push-Location -LiteralPath $bundle
        try {
            $env:PATH = (Join-Path $env:SystemRoot 'System32') + [IO.Path]::PathSeparator + $env:SystemRoot
            $env:VULKAN_SDK = $null; $env:VK_LAYER_PATH = $null; $env:VK_INSTANCE_LAYERS = $null
            # Native DLL discovery now has only the application directory and Windows.
            # This does not change adapters/firewall or claim the network was disconnected.
            Invoke-NgenNative -FilePath (Join-Path $bundle 'flight_demo.exe') -Arguments @(
                (Join-Path $bundle 'assets/ship.ngas'), '--verify') -LogPath "$prefix.offline-verify.txt"
            Invoke-NgenNative -FilePath (Join-Path $bundle 'flight_demo.exe') -Arguments @(
                (Join-Path $bundle 'assets/ship.ngas'), '--replay', '--frames=3', '--warmup=2', '--mute',
                "--report=$prefix.offline.csv") -LogPath "$prefix.offline.txt"
        } finally {
            Pop-Location
            foreach ($name in $saved.Keys) {
                if ($null -eq $saved[$name]) {
                    # A plain $null binds to an empty string on current PowerShell/.NET.
                    [Environment]::SetEnvironmentVariable($name, [NullString]::Value, 'Process')
                } else {
                    [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process')
                }
            }
        }
    }
}

function Invoke-NgenRenderer {
    param(
        [Parameter(Mandatory)][string]$BuildDirectory,
        [Parameter(Mandatory)][string]$LogDirectory,
        [Parameter(Mandatory)][string]$Configuration,
        [Parameter(Mandatory)][scriptblock]$Checks,
        [switch]$Scene,
        [switch]$Inspector,
        [string]$ScenePackage = ''
    )
    if ($Inspector -and -not $ScenePackage) { throw 'Inspector verification requires ScenePackage.' }
    if ($ScenePackage) { $Scene = $true }
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
        if ($Scene) {
            $sceneProbe = Join-Path $BuildDirectory 'scene_probe.exe'
            Invoke-NgenNative -FilePath $sceneProbe -LogPath (Join-Path $LogDirectory "$Configuration-scene.txt")
            if ($ScenePackage) {
                $identity = Get-NgenSceneIdentity -Path $ScenePackage | ConvertTo-Json
                $identity | Set-Content -LiteralPath (Join-Path $LogDirectory "$Configuration-scene-package.json")
                $capture = Join-Path $LogDirectory "$Configuration-scene-package.bmp"
                if (Test-Path -LiteralPath $capture) { throw 'Scene capture path is not fresh.' }
                Invoke-NgenNative -FilePath $sceneProbe -Arguments @($ScenePackage, $capture) -LogPath (Join-Path $LogDirectory "$Configuration-scene-package.txt")
                if (-not (Test-Path -LiteralPath $capture -PathType Leaf) -or (Get-Item -LiteralPath $capture).Length -eq 0) {
                    throw 'Scene probe completed without the requested image evidence.'
                }
                if ((Get-NgenSceneIdentity -Path $ScenePackage | ConvertTo-Json) -cne $identity) {
                    throw 'ScenePackage changed during native verification.'
                }
            }
        }
        if ($Inspector) {
            Invoke-NgenNative -FilePath (Join-Path $BuildDirectory 'inspector_probe.exe') -LogPath (Join-Path $LogDirectory "$Configuration-inspector.txt")
            $modes = @('inspection')
            if ($Configuration -eq 'Release') { $modes += @('steady', 'stream') }
            foreach ($mode in $modes) {
                $capture = Join-Path $LogDirectory "$Configuration-$mode.bmp"
                $report = Join-Path $LogDirectory "$Configuration-$mode.csv"
                if ((Test-Path -LiteralPath $capture) -or (Test-Path -LiteralPath $report) -or
                    (Test-Path -LiteralPath (Join-Path $LogDirectory "$Configuration-$mode.artifacts.json"))) { throw 'Inspector workload outputs are not fresh.' }
                $frames = if ($mode -eq 'inspection') { 1200 } else { 600 }
                $arguments = @($ScenePackage, '--inspector', "--frames=$frames", '--warmup=60', "--capture=$capture", "--report=$report")
                if ($mode -eq 'inspection') { $arguments += @('--walkthrough', '--reload-cycles=2', '--validation') }
                if ($mode -eq 'stream') { $arguments += @('--walkthrough', '--reload-cycles=4', '--no-resize') }
                Invoke-NgenWorkload -FilePath (Join-Path $BuildDirectory 'scene_workload.exe') -Arguments $arguments -LogPath (Join-Path $LogDirectory "$Configuration-$mode.txt") -MemoryPath (Join-Path $LogDirectory "$Configuration-$mode.working-set.json")
                $artifacts = foreach ($path in @($capture, $report)) {
                    $item = Get-Item -LiteralPath $path -Force -ErrorAction Stop
                    if ($item -isnot [IO.FileInfo] -or ($item.Attributes -band [IO.FileAttributes]::ReparsePoint) -or $item.Length -lt 1 -or $item.Length -gt 8388608) { throw 'Inspector workload capture/report is missing, linked, empty or oversized.' }
                    [ordered]@{ name = $item.Name; bytes = $item.Length; sha256 = (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() }
                }
                $artifacts | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $LogDirectory "$Configuration-$mode.artifacts.json")
                if ((Get-NgenSceneIdentity -Path $ScenePackage | ConvertTo-Json) -cne $identity) { throw 'ScenePackage changed during inspector workload.' }
            }
        }
        $manifests = @(Get-ChildItem -LiteralPath (Join-Path $BuildDirectory 'shaders') -Filter 'manifest.json' -Recurse -File)
        if ($manifests.Count -ne 1) { throw 'Expected one immutable shader package in the fresh build.' }
        Copy-Item -LiteralPath $manifests[0].FullName -Destination (Join-Path $LogDirectory "$Configuration-shader-manifest.json")
        foreach ($shader in @('cull', 'pbr-vertex', 'pbr-fragment', 'shadow-vertex', 'shadow-fragment', 'post-vertex', 'post-fragment', 'overlay-vertex', 'overlay-fragment')) {
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

# Flight adds physics/audio to the same GUI, renderer and checked scene graph.
if ($FlightDemo) { $Inspector = $true }
if ($Inspector) { $Scene = $true }
if ($ScenePackage) { $Scene = $true }
if ($Scene) { $Renderer = $true }

if (-not $IsWindows) {
    [Console]::Error.WriteLine('This build script requires Windows and PowerShell 7.2 or newer.')
    exit 2
}

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$run = if ($DiagnoseGpuMemoryProcessId) {
    Join-Path $root ('build/windows-gpu-diagnostic/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [guid]::NewGuid().ToString('N'))
} elseif ($MeasureWorkload) {
    Join-Path $root ('build/windows-measure/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '-' + [guid]::NewGuid().ToString('N'))
} else { Join-Path $root ('build/windows/' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff')) }
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
    if ($DiagnoseGpuMemoryProcessId) {
        Get-NgenGpuMemoryDiagnostic -ProcessId $DiagnoseGpuMemoryProcessId -ComparePdh |
            ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $logs 'gpu-memory-diagnostic.json')
        Write-Host 'Read-only PDH/CIM provider comparison retained. Only the owned typeperf helpers have a 2 s deadline; the selected demo is never stopped. This is not memory-budget acceptance.'
        exit 0
    }
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

    if ($FlightDemo -and -not $ScenePackage) { throw 'Flight verification requires -ScenePackage.' }
    if ($Inspector -and -not $ScenePackage) { throw 'Inspector verification requires -ScenePackage.' }
    if ($ScenePackage) {
        Get-NgenSceneIdentity -Path $ScenePackage | Out-Null
        $ScenePackage = (Resolve-Path -LiteralPath $ScenePackage).ProviderPath
    }
    if ($MeasureWorkload) {
        Invoke-NgenMeasurement -BuildDirectory $MeasureWorkload -LogDirectory $logs -ScenePackage $ScenePackage
        Write-Host 'Existing Release workload memory observation completed without rebuilding. Review the sampled WDDM scope and actual records; this is not personal or formal M2 acceptance.'
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
    if ($FlightDemo) {
        Invoke-NgenNative -FilePath $conan -Arguments @('export', 'recipes/joltphysics', '--version=5.6.0', '--user=ngen', '--channel=headless-double') -LogPath (Join-Path $logs 'jolt-recipe.txt')
    }
    $lockfile = if ($FlightDemo) { 'conan-demo.lock' } elseif ($Inspector) { 'conan-inspector.lock' } elseif ($Scene) { 'conan-scene.lock' } elseif ($Renderer) { 'conan-render.lock' } else { 'conan.lock' }
    $inspectorOption = if ($Inspector) { 'True' } else { 'False' }
    $inspectorBuild = if ($Inspector) { 'ON' } else { 'OFF' }
    $assetsOption = if ($Scene) { 'True' } else { 'False' }
    $assetsBuild = if ($Scene) { 'ON' } else { 'OFF' }
    $rendererOption = if ($Renderer) { 'True' } else { 'False' }
    $rendererBuild = if ($Renderer) { 'ON' } else { 'OFF' }
    $flightOption = if ($FlightDemo) { 'True' } else { 'False' }
    $flightBuild = if ($FlightDemo) { 'ON' } else { 'OFF' }
    foreach ($config in $configurations) {
        $build = Join-Path $run $config
        $buildType = if ($Renderer) { 'Release' } else { $config }
        Invoke-NgenNative -FilePath $conan -Arguments @(
            'install', '.', '-pr:h', $profile, '-pr:b', $buildProfile,
            '-s:h', 'compiler.cppstd=20', '-s:b', 'compiler.cppstd=20',
            '-s:h', "build_type=$config", '-s:b', "build_type=$buildType",
            '-c:h', 'tools.cmake.cmaketoolchain:generator=Ninja',
            '-c:b', 'tools.cmake.cmaketoolchain:generator=Ninja',
            # The x64 developer shell above is already active. Re-entering vcvars
            # inside cmd.exe can overflow its command/environment line limit.
            '-c:h', 'tools.microsoft.msbuild:installation_path=',
            '-c:b', 'tools.microsoft.msbuild:installation_path=',
            '-c:h', 'tools.build:jobs=2', '-c:b', 'tools.build:jobs=2',
            '-o:h', "&:renderer=$rendererOption", '-o:h', "&:shaders=$rendererOption", '-o:h', "&:assets=$assetsOption", '-o:h', "&:inspector=$inspectorOption",
            '-o:h', "&:physics=$flightOption", '-o:h', "&:audio=$flightOption",
            '--lockfile', $lockfile, '--build=missing', '-of', $build
        ) -LogPath (Join-Path $logs "$config-conan.txt")
        $toolchain = Join-Path $build 'conan_toolchain.cmake'
        Invoke-NgenNative -FilePath $cmake -Arguments @(
            '-S', $root, '-B', $build, '-G', 'Ninja',
            "-DCMAKE_TOOLCHAIN_FILE=$toolchain", "-DCMAKE_BUILD_TYPE=$config",
            "-DNGEN_BUILD_RENDERER=$rendererBuild", "-DNGEN_BUILD_ASSETS=$assetsBuild", "-DNGEN_BUILD_INSPECTOR=$inspectorBuild",
            "-DNGEN_BUILD_PHYSICS=$flightBuild", "-DNGEN_BUILD_AUDIO=$flightBuild", "-DNGEN_BUILD_FLIGHT_DEMO=$flightBuild"
        ) -LogPath (Join-Path $logs "$config-configure.txt")
        # Keep diagnostics from all independent translation units; any error still fails the run.
        Invoke-NgenNative -FilePath $cmake -Arguments @('--build', $build, '-j', '2', '--', '-k', '0') -LogPath (Join-Path $logs "$config-build.txt")
        $checks = {
            Invoke-NgenNative -FilePath $ctest -Arguments @('--test-dir', $build, '--show-only') -LogPath (Join-Path $logs "$config-test-list.txt")
            Invoke-NgenNative -FilePath $ctest -Arguments @('--test-dir', $build, '--output-on-failure', '--no-tests=error') -LogPath (Join-Path $logs "$config-tests.txt")
            $harness = Join-Path $build 'sim_harness.exe'
            Invoke-NgenNative -FilePath $harness -LogPath (Join-Path $logs "$config-serial.txt")
            Invoke-NgenNative -FilePath $harness -Arguments @('--workers=2') -LogPath (Join-Path $logs "$config-workers.txt")
        }
        if ($FlightDemo) {
            # Keep prerequisite fixture checks; do not rerun M2 inspector/appearance acceptance.
            Invoke-NgenRenderer -BuildDirectory $build -LogDirectory $logs -Configuration $config -Checks $checks
            Invoke-NgenFlight -BuildDirectory $build -LogDirectory $logs -Configuration $config -ScenePackage $ScenePackage -Python $python -SourceRoot $root
        } elseif ($Renderer) {
            Invoke-NgenRenderer -BuildDirectory $build -LogDirectory $logs -Configuration $config -Checks $checks -Scene:$Scene -Inspector:$Inspector -ScenePackage $ScenePackage
        } else {
            & $checks
        }
    }
    if ($FlightDemo) {
        Write-Host "Windows $Compiler flight checks completed. Release records separate 1080p clean replay and sampled process-memory runs, plus an app-local bundle startup without SDK paths. Review actual budgets, network-disconnected startup, listening and controls separately; this is not owner acceptance. The private bundle stays outside evidence.zip."
    } elseif ($Inspector) {
        Write-Host "Windows $Compiler inspector/bloom/reload validation completed. Release additionally records fixed-resolution steady/streaming CSVs and observed process working set. Inspect actual budgets, captures and GPU-memory scope; this is not personal visual or formal M2 acceptance."
    } elseif ($ScenePackage) {
        Write-Host "Windows $Compiler CPU, renderer/shader, built-in fixture and supplied checked-package upload/PBR/frame-pin checks passed. See the package hash and separate scene-package logs. This is not final visual, performance or M2 acceptance."
    } elseif ($Scene) {
        Write-Host "Windows $Compiler CPU, renderer/shader, checked-scene fixture upload/PBR/frame-pin checks passed. This does not validate Windows frigate rendering, final performance or full M2 acceptance."
    } elseif ($Renderer) {
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
