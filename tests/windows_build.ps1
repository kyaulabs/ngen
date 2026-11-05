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

    & $pwsh -NoProfile -File $scriptPath -Configuration Invalid *> $log
    if ($LASTEXITCODE -eq 0) { throw 'Invalid build configuration was accepted' }
    if (-not $IsWindows) {
        & $pwsh -NoProfile -File $scriptPath *> $log
        if ($LASTEXITCODE -ne 2 -or (Get-Content -Raw $log) -notmatch 'requires Windows') {
            throw 'Unsupported host did not fail before build preparation'
        }
    }
    $failed = $false
    try {
        Invoke-NgenNative -FilePath (Join-Path $directory 'missing.exe') -LogPath $log
    } catch {
        $failed = $true
    }
    if (-not $failed) { throw 'A missing executable was reported as success' }
    Write-Host 'PASS: parsing, process failure, captured output, spaced arguments and input/platform validation'
} finally {
    Remove-Item -LiteralPath $directory -Recurse -Force
}
# Expected failing subprocesses must not become the CI shell's final exit code.
exit 0
