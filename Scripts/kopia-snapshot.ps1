# Snapshot of the whole project folder into the Kopia repository on the external disk.
# Run by the scheduled task "Nevergone Kopia Snapshot" (weekly trigger and disk-arrival
# trigger, elevated, so Kopia uses VSS). Start it with Scripts/kopia-snapshot-manual.ps1.
# Exits 0 without error (and logs "skipped") when the repository is not reachable.
# -WaitForDisk: poll up to 60 s for the repository, because the drive letter is
# mounted a few seconds after the disk-arrival event fires.
# Every run ends with exactly one terminal log line: "skipped: ...", "error: ..." or
# "finished with exit code N" (the manual script waits for it).

param([switch]$WaitForDisk)

$ErrorActionPreference = 'Stop'

. (Join-Path $PSScriptRoot 'kopia-config.ps1')

try {
    if ($WaitForDisk) {
        $Deadline = (Get-Date).AddSeconds(60)
        while (-not (Test-KopiaRepo) -and (Get-Date) -lt $Deadline) {
            Start-Sleep -Seconds 3
        }
    }
    if (-not (Test-KopiaRepo)) {
        Write-Log "skipped: Kopia repository $RepoPath not available (external disk disconnected)"
        exit 0
    }

    $Project = Split-Path -Parent $PSScriptRoot
    $Kopia = Get-ChildItem "$env:LOCALAPPDATA\Microsoft\WinGet\Packages\Kopia.KopiaCLI_*" -Recurse -Filter kopia.exe |
        Select-Object -First 1 -ExpandProperty FullName
    if (-not $Kopia) {
        Write-Log 'error: kopia.exe not found'
        exit 1
    }

    # Kopia writes progress to stderr; do not let PowerShell turn that into errors.
    $ErrorActionPreference = 'Continue'
    $Output = @(& $Kopia snapshot create $Project 2>&1)
    $Code = $LASTEXITCODE
    $Output | Select-Object -Last 4 | ForEach-Object { Write-Log "kopia: $_" }
    $Vss = if ($Output -match 'creating volume shadow copy') { 'used' } else { 'NOT used' }
    Write-Log "vss: $Vss"
    Write-Log "finished with exit code $Code"
    exit $Code
}
catch {
    Write-Log "error: $($_.Exception.Message)"
    exit 1
}
