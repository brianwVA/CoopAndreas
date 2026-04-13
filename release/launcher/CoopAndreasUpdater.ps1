param(
    [string]$RepoOwner = "brianwVA",
    [string]$RepoName = "CoopAndreas",
    [string]$Branch = "old-0.2.2",
    [string]$PackagePath = "release/old-0.2.3",
    [switch]$RunAfterUpdate
)

$ErrorActionPreference = "Stop"
$gtaDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$tempRoot = Join-Path $env:TEMP ("coopandreas_updater_" + [guid]::NewGuid().ToString("N"))
$zipPath = Join-Path $tempRoot "repo.zip"

function Write-Info([string]$m) { Write-Host "[INFO] $m" -ForegroundColor Cyan }
function Write-Ok([string]$m) { Write-Host "[OK]   $m" -ForegroundColor Green }
function Write-Err([string]$m) { Write-Host "[ERR]  $m" -ForegroundColor Red }

try {
    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    $zipUrl = "https://codeload.github.com/$RepoOwner/$RepoName/zip/refs/heads/$Branch"
    Write-Info "Pobieram paczke: $zipUrl"
    Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath -UseBasicParsing

    Write-Info "Rozpakowuje paczke..."
    Expand-Archive -Path $zipPath -DestinationPath $tempRoot -Force

    $repoRoot = Get-ChildItem -Path $tempRoot -Directory | Where-Object { $_.Name -like "$RepoName-*" } | Select-Object -First 1
    if (-not $repoRoot) { throw "Nie znaleziono katalogu repo po rozpakowaniu." }

    $pkg = Join-Path $repoRoot.FullName ($PackagePath -replace '/', '\\')
    if (-not (Test-Path $pkg)) {
        throw "Brak folderu paczki '$PackagePath' w branchu '$Branch'."
    }

    $files = @("CoopAndreasSA.dll", "server.exe", "proxy.dll", "VERSION.txt")
    $backupDir = Join-Path $gtaDir ("CoopAndreas_backup_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
    New-Item -ItemType Directory -Path $backupDir -Force | Out-Null

    foreach ($f in $files) {
        $src = Join-Path $pkg $f
        if (-not (Test-Path $src)) { throw "Brak pliku w paczce: $f" }

        $dst = Join-Path $gtaDir $f
        if (Test-Path $dst) {
            Copy-Item -LiteralPath $dst -Destination (Join-Path $backupDir $f) -Force
        }

        Copy-Item -LiteralPath $src -Destination $dst -Force
        Write-Ok "Podmieniono: $f"
    }

    Write-Ok "Aktualizacja zakonczona pomyslnie."

    if ($RunAfterUpdate) {
        $serverExe = Join-Path $gtaDir "server.exe"
        $gtaExe = Join-Path $gtaDir "gta_sa.exe"

        if (Test-Path $serverExe) {
            Start-Process -FilePath $serverExe -WorkingDirectory $gtaDir -WindowStyle Normal
            Start-Sleep -Seconds 1
        }

        if (Test-Path $gtaExe) {
            Start-Process -FilePath $gtaExe -WorkingDirectory $gtaDir -WindowStyle Normal
        } else {
            Write-Err "Nie znaleziono gta_sa.exe w: $gtaDir"
        }
    }

    exit 0
}
catch {
    Write-Err $_.Exception.Message
    exit 1
}
finally {
    if (Test-Path $tempRoot) {
        Remove-Item -LiteralPath $tempRoot -Recurse -Force -ErrorAction SilentlyContinue
    }
}
