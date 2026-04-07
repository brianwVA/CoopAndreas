param(
    [string]$RepoRoot = "C:\CoopAndreas",
    [string]$GamePath = "C:\GTA San Andreas",
    [string]$SannyDir = "C:\Tools\SannyBuilder4"
)

$ErrorActionPreference = "Stop"

New-Item -ItemType Directory -Path $SannyDir -Force | Out-Null

$settingsFile = Join-Path $SannyDir "data\settings.ini"
$sdkSource = Join-Path $RepoRoot "sdk\Sanny Builder 4"

if (-not (Test-Path $sdkSource)) {
    throw "CoopAndreas Sanny SDK files not found at $sdkSource"
}

Copy-Item (Join-Path $sdkSource "*") $SannyDir -Recurse -Force

if (Test-Path $settingsFile) {
    $content = Get-Content $settingsFile -Raw
    if ($content -match "(?m)^EditMode=") {
        $content = [regex]::Replace($content, "(?m)^EditMode=.*$", "EditMode=sa_sbl_coopandreas")
    } else {
        $content += "`r`nEditMode=sa_sbl_coopandreas"
    }

    if ($content -match "(?m)^GamePath=") {
        $escapedGamePath = $GamePath -replace '\\', '\\'
        $content = [regex]::Replace($content, "(?m)^GamePath=.*$", "GamePath=$escapedGamePath")
    } else {
        $content += "`r`nGamePath=$GamePath"
    }

    Set-Content -Path $settingsFile -Value $content -Encoding ASCII
}

Write-Output "Sanny Builder configured."
Write-Output "Mode: sa_sbl_coopandreas"
Write-Output "GamePath: $GamePath"
