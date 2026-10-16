# Shared settings for the Kopia snapshot scripts (dot-sourced, not run directly).

$RepoPath = 'G:\KopiaRepo-Nevergone'
$LogFile = Join-Path $env:PUBLIC 'kopia-snapshot.log'
$TaskName = 'Nevergone Kopia Snapshot'

# The drive letter alone proves nothing: another USB device can take G:.
# A filesystem Kopia repository holds a kopia.repository blob in its root; the filesystem
# backend stores it with a shard extension (kopia.repository.f), hence the wildcard.
function Test-KopiaRepo {
    # String concatenation, not Join-Path: Join-Path throws when the drive letter does not exist.
    Test-Path -Path "$RepoPath\kopia.repository*" -PathType Leaf
}

function Write-Log($Message) {
    "$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') $Message" | Add-Content -Path $LogFile
}
