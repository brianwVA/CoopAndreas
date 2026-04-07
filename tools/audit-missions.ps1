param(
    [string]$RepoRoot = "C:\CoopAndreas"
)

$ErrorActionPreference = "Stop"

$scriptDir = Join-Path $RepoRoot "scm\scripts"

Get-ChildItem $scriptDir -Filter *.txt | ForEach-Object {
    $content = Get-Content $_.FullName -Raw
    $missionHeader = [regex]::Match($content, '(?m)^//-------------Mission\s+(\d+)---------------')
    $missionName = [regex]::Match($content, '(?m)^// Originally:\s*(.+)$')

    [pscustomobject]@{
        File = $_.Name
        MissionId = if ($missionHeader.Success) { [int]$missionHeader.Groups[1].Value } else { $null }
        MissionName = if ($missionName.Success) { $missionName.Groups[1].Value.Trim() } else { "" }
        HasSyncScript = $content.Contains("Coop.EnableSyncingThisScript()")
        HasPlayerCollection = $content.Contains("Coop.CollectNetworkPlayersForTheMission()")
        HasPerPlayerCheckpoint = $content.Contains("Coop.UpdateCheckpointForNetworkPlayer(")
        HasSafeTeleport = $content.Contains("Coop.TeleportPlayersToHostSafely(")
    }
} | Sort-Object MissionId, File | Format-Table -AutoSize
