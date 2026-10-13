# Snapshot of the whole project folder into the Kopia repository on the external disk.
# Used by the scheduled task (hourly) and for manual snapshots.
# Exits 0 without error when the external disk is disconnected.

$ErrorActionPreference = 'Stop'

$RepoPath = 'G:\KopiaRepo-Nevergone'
$Project = Split-Path -Parent $PSScriptRoot
$Kopia = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Kopia.KopiaCLI_*" -Recurse -Filter kopia.exe |
    Select-Object -First 1 -ExpandProperty FullName
$LogFile = Join-Path $env:PUBLIC 'kopia-snapshot.log'

function Write-Log($Message) {
    "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') $Message" | Add-Content -Path $LogFile
}

if (-not (Test-Path $RepoPath)) {
    Write-Log "skipped: $RepoPath not available (external disk disconnected)"
    exit 0
}
if (-not $Kopia) {
    Write-Log 'error: kopia.exe not found'
    exit 1
}

# Kopia writes progress to stderr; do not let PowerShell turn that into errors.
$ErrorActionPreference = 'Continue'
$Output = & $Kopia snapshot create $Project 2>&1
$Code = $LASTEXITCODE
$Output | Select-Object -Last 4 | ForEach-Object { Write-Log "kopia: $_" }
Write-Log "finished with exit code $Code"
exit $Code
