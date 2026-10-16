# Manual snapshot: starts the elevated scheduled task (so Kopia uses VSS) and waits for it.
# Works from a normal, non-admin shell.
# Exit code: 0 = snapshot recorded, 2 = skipped (repository not reachable), 1 = failure.

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'kopia-config.ps1')

if (-not (Test-KopiaRepo)) {
    Write-Output "SKIPPED: Kopia repository $RepoPath not available. Connect the backup disk and run again."
    exit 2
}

# With IgnoreNew, starting a task that is already running does nothing, and the log would
# belong to the other run (boot or disk-arrival trigger). Refuse instead of misreporting.
if ((Get-ScheduledTask -TaskName $TaskName).State -eq 'Running') {
    Write-Output "FAILED: task '$TaskName' is already running (disk-arrival or scheduled run). Wait for it and run again."
    exit 1
}

function Get-NewLogLines {
    if (Test-Path $LogFile) { @(Get-Content $LogFile | Select-Object -Skip $LinesBefore) } else { @() }
}

$LinesBefore = if (Test-Path $LogFile) { @(Get-Content $LogFile).Count } else { 0 }
Start-ScheduledTask -TaskName $TaskName

# Terminal lines written by kopia-snapshot.ps1 itself (anchored, so "kopia: ..." lines never match).
$Terminal = '^\d{4}-\d\d-\d\d \d\d:\d\d:\d\d (skipped:|error:|finished with exit code (\d+))'
$Deadline = (Get-Date).AddMinutes(125)   # the task has a 2 h execution limit
$Started = Get-Date
$Done = $null
$New = @()
while ((Get-Date) -lt $Deadline) {
    Start-Sleep -Seconds 2
    $New = Get-NewLogLines
    $Done = $New | Where-Object { $_ -match $Terminal } | Select-Object -Last 1
    if ($Done) { break }
    # Task ended without a terminal line (crash before logging): stop waiting.
    if (((Get-Date) - $Started).TotalSeconds -gt 15 -and (Get-ScheduledTask -TaskName $TaskName).State -ne 'Running') {
        Start-Sleep -Seconds 2
        $New = Get-NewLogLines
        $Done = $New | Where-Object { $_ -match $Terminal } | Select-Object -Last 1
        break
    }
}

$New | ForEach-Object { Write-Output $_ }
if (-not $Done) {
    $Result = (Get-ScheduledTaskInfo -TaskName $TaskName).LastTaskResult
    Write-Output "FAILED: no final log line from the task (task result $Result, or timed out). Check $LogFile."
    exit 1
}
if ($Done -match ' skipped:') {
    Write-Output 'SKIPPED: the task did not find the Kopia repository. Connect the backup disk and run again.'
    exit 2
}
if ($Done -match 'finished with exit code 0$') {
    $Vss = if ($New -match ' vss: used') { 'with VSS' } else { 'WITHOUT VSS' }
    Write-Output "SNAPSHOT RECORDED ($Vss)."
    exit 0
}
Write-Output 'FAILED: see the log lines above.'
exit 1
