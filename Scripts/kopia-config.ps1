# Shared settings for the Kopia snapshot scripts (dot-sourced, not run directly).

$RepoPath = 'G:\KopiaRepo-Nevergone'
$LogFile = Join-Path $env:PUBLIC 'kopia-snapshot.log'
$TaskName = 'Nevergone Kopia Snapshot'

# The drive letter alone proves nothing: another USB device can take G:.
# A filesystem Kopia repository always holds a kopia.repository blob in its root.
function Test-KopiaRepo {
    # String concatenation, not Join-Path: Join-Path throws when the drive letter does not exist.
    Test-Path -LiteralPath "$RepoPath\kopia.repository"
}

function Write-Log($Message) {
    "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') $Message" | Add-Content -Path $LogFile
}
