#Requires -Version 7.2
$ErrorActionPreference = 'Stop'
$tokens=$null; $errors=$null
$path=Join-Path $PSScriptRoot '../tools/build-windows.ps1'
$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if($errors.Count){throw($errors|Out-String)}
foreach($name in @('Get-NgenPdhProcessMemory','Close-NgenPdhProcessMemory')){
    $definition=$ast.Find({param($node)$node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name},$true)
    if(-not $definition){throw "Missing native PDH process boundary: $name"}
    . ([scriptblock]::Create($definition.Extent.Text))
}
if(-not $IsWindows){
    $folder=Join-Path ([IO.Path]::GetTempPath()) ('ngen native reader '+[guid]::NewGuid())
    New-Item -ItemType Directory -Path $folder|Out-Null
    $file=Join-Path $folder 'gpu_memory_observer.exe'
    $reader=$null
    try{
        [IO.File]::WriteAllText($file,@'
#!/usr/bin/env python3
import json,sys
assert sys.argv[1:] == ['42']
print('{"schema":1,"ready":true}',flush=True)
for line in sys.stdin:
    assert line.strip()=='read'
    print(json.dumps({'status':'observed','pdh_status':0,'rows':[{'Name':'pid_42_luid_0x0_0x1_phys_0','DedicatedUsage':128,'SharedUsage':16,'TotalCommitted':160}]}),flush=True)
'@)
        & chmod +x $file
        if($LASTEXITCODE){throw 'Native-reader fixture could not be prepared.'}
        $rows=@(Get-NgenPdhProcessMemory -FilePath $file -ProcessId 42 -Reader ([ref]$reader))
        if($rows.Count -ne 1 -or $rows[0].DedicatedUsage -ne 128 -or $rows[0].SharedUsage -ne 16 -or $rows[0].TotalCommitted -ne 160){
            throw 'The persistent PDH transport did not publish the owned native snapshot.'
        }
        $helperId=$reader.Id
        $rows=@(Get-NgenPdhProcessMemory -FilePath $file -ProcessId 42 -Reader ([ref]$reader))
        if($reader.Id -ne $helperId -or $rows.Count -ne 1){throw 'The PDH query was not reused across sample points.'}
        Close-NgenPdhProcessMemory -Reader ([ref]$reader)
        if($reader -or (Get-Process -Id $helperId -ErrorAction SilentlyContinue)){throw 'The native measurement helper escaped shutdown.'}
        [IO.File]::WriteAllText($file,@'
#!/usr/bin/env python3
import sys
print('{"schema":1,"ready":true}',flush=True)
for line in sys.stdin: print('{"status":"observed","pdh_status":0,"rows":[]}',flush=True)
'@)
        $rejected=$false
        try { Get-NgenPdhProcessMemory -FilePath $file -ProcessId 42 -Reader ([ref]$reader)|Out-Null } catch { $rejected=$true }
        if(-not $rejected -or $reader){throw 'An observed-but-empty native frame was accepted or its helper escaped.'}
        [IO.File]::WriteAllText($file,@'
#!/usr/bin/env python3
import sys
print('{"schema":1,"ready":true}',flush=True)
for line in sys.stdin: print('{"status":"error","pdh_status":3221228486,"kind":"counter","rows":[]}',flush=True)
'@)
        $nativeStatus=$null
        try { Get-NgenPdhProcessMemory -FilePath $file -ProcessId 42 -Reader ([ref]$reader)|Out-Null }
        catch { $nativeStatus=$_.Exception.Data['PdhStatus'] }
        if($nativeStatus -ne 3221228486 -or $reader){throw 'Native PDH failure status or failed-reader cleanup was lost.'}
        foreach($reply in @('sys.exit(0)', "print('x'*32769,flush=True)")){
            [IO.File]::WriteAllText($file,"#!/usr/bin/env python3`nimport sys`nprint('{`"schema`":1,`"ready`":true}',flush=True)`nfor line in sys.stdin: $reply`n")
            $rejected=$false
            try { Get-NgenPdhProcessMemory -FilePath $file -ProcessId 42 -Reader ([ref]$reader)|Out-Null } catch { $rejected=$true }
            if(-not $rejected -or $reader){throw 'EOF or oversized native output was accepted or leaked its helper.'}
        }
        $pidFile=Join-Path $folder 'helper.pid'
        $pidLiteral=$pidFile|ConvertTo-Json -Compress
        [IO.File]::WriteAllText($file,"#!/usr/bin/env python3`nimport os,sys,time`nfrom pathlib import Path`nPath($pidLiteral).write_text(str(os.getpid()))`nprint('{`"schema`":1,`"ready`":true}',flush=True)`nfor line in sys.stdin: time.sleep(10)`n")
        $clock=[Diagnostics.Stopwatch]::StartNew();$timedOut=$false
        try { Get-NgenPdhProcessMemory -FilePath $file -ProcessId 42 -Reader ([ref]$reader)|Out-Null }
        catch { $timedOut=$_.Exception -is [TimeoutException] }
        if(-not $timedOut -or $clock.ElapsedMilliseconds -gt 5000 -or $reader){throw 'A blocked PDH read escaped its 2 s request budget or ownership.'}
        $helperId=[int](Get-Content -LiteralPath $pidFile -Raw)
        if(Get-Process -Id $helperId -ErrorAction SilentlyContinue){throw 'The blocked PDH helper remained alive.'}
    }finally{
        Close-NgenPdhProcessMemory -Reader ([ref]$reader) -Abort
        Remove-Item -LiteralPath $folder -Recurse -Force
    }
}
Write-Host 'Native PDH transport boundaries pass; not native Windows counter execution.'
exit 0
