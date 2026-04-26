# Runs incremental build + ctest when an edited file path is .c or .h (see stdin JSON from Cursor afterFileEdit).
$ErrorActionPreference = "Stop"
$raw = [Console]::In.ReadToEnd()
if ([string]::IsNullOrWhiteSpace($raw)) {
    exit 0
}

function Test-IsSourceFilePath {
    param([string]$s)
    if ([string]::IsNullOrEmpty($s)) { return $false }
    if ($s -notmatch "\.(c|h)$") { return $false }
    return $s -match "[/\\]" -or $s -match "^[A-Za-z]:"
}

function Test-Walk {
    param($o)
    if ($null -eq $o) { return $false }
    if ($o -is [string]) { return (Test-IsSourceFilePath $o) }
    if ($o -is [System.Collections.IEnumerable] -and -not ($o -is [string])) {
        foreach ($x in $o) {
            if (Test-Walk $x) { return $true }
        }
        return $false
    }
    if ($o.PSObject -and $o.PSObject.Properties) {
        foreach ($p in $o.PSObject.Properties) {
            if (Test-Walk $p.Value) { return $true }
        }
    }
    return $false
}

$run = $false
try {
    $j = $raw | ConvertFrom-Json
    if (Test-Walk $j) { $run = $true }
} catch {
    if ($raw -cmatch "src[/\\].*?\.(c|h)(`"|\\|$)") { $run = $true }
    elseif ($raw -cmatch "tests[/\\].*?\.(c|h)(`"|\\|$)") { $run = $true }
    elseif ($raw -cmatch "apps[/\\].*?\.(c|h)(`"|\\|$)") { $run = $true }
    elseif ($raw -cmatch '([A-Za-z]:[\\/][^":]+\.(c|h))') { $run = $true }
}
if (-not $run) {
    exit 0
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$bat = Join-Path $repoRoot "verify-incremental.bat"
if (-not (Test-Path -LiteralPath $bat)) {
    exit 0
}
$p = Start-Process -FilePath "cmd.exe" -ArgumentList @("/c", "call `"$bat`"") -NoNewWindow -Wait -PassThru
exit $p.ExitCode
