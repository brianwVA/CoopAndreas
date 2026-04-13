param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path

function Write-Info([string]$m) { Write-Host "[INFO] $m" -ForegroundColor Cyan }
function Write-Ok([string]$m) { Write-Host "[OK]   $m" -ForegroundColor Green }
function Write-Err([string]$m) { Write-Host "[ERR]  $m" -ForegroundColor Red }

function Resolve-GtaDir([string]$startDir) {
    $current = Resolve-Path $startDir
    for ($i = 0; $i -lt 6; $i++) {
        $candidate = Join-Path $current "gta_sa.exe"
        if (Test-Path $candidate) {
            return $current
        }

        $parent = Split-Path -Parent $current
        if ([string]::IsNullOrWhiteSpace($parent) -or $parent -eq $current) {
            break
        }
        $current = $parent
    }

    throw "Nie znaleziono gta_sa.exe. Umiesc cleaner w katalogu GTA (lub podkatalogu) i uruchom ponownie."
}

function Stop-IfRunning([string[]]$processNames) {
    foreach ($procName in $processNames) {
        $running = Get-Process -Name $procName -ErrorAction SilentlyContinue
        if ($running) {
            Write-Info "Wykryto uruchomiony proces '$procName'. Zatrzymuje..."
            foreach ($p in $running) {
                Stop-Process -Id $p.Id -Force -ErrorAction Stop
            }
            Start-Sleep -Milliseconds 700
            Write-Ok "Zatrzymano: $procName"
        }
    }
}

function Restore-SampFiles([string]$gameDir) {
    $entries = @(
        "samp.dll",
        "samp.exe",
        "samp.saa",
        "SAMP\\samp.dll",
        "SAMP\\samp.exe"
    )

    foreach ($entry in $entries) {
        $disabled = Join-Path $gameDir "$entry.coop_disabled"
        $normal = Join-Path $gameDir $entry
        if (Test-Path $disabled) {
            if (Test-Path $normal) {
                Remove-Item -LiteralPath $normal -Force -ErrorAction SilentlyContinue
            }
            Move-Item -LiteralPath $disabled -Destination $normal -Force
            Write-Ok "Przywrocono SA:MP plik: $entry"
        }
    }
}

function Remove-FileIfExists([string]$path) {
    if (Test-Path $path) {
        Remove-Item -LiteralPath $path -Force
        Write-Ok "Usunieto: $path"
    }
}

try {
    $gtaDir = Resolve-GtaDir -startDir $scriptDir

    if (-not $Force) {
        Write-Host "To odinstaluje CoopAndreas, fix rozdzielczosci i pliki launcherow z: $gtaDir" -ForegroundColor Yellow
        $confirm = Read-Host "Kontynuowac? [tak/N]"
        if ($confirm -notin @("tak", "TAK", "t", "T", "yes", "y", "Y")) {
            throw "Odinstalowanie przerwane przez uzytkownika."
        }
    }

    Stop-IfRunning -processNames @("server", "gta_sa")
    Restore-SampFiles -gameDir $gtaDir

    $filesToRemove = @(
        "CoopAndreasSA.dll",
        "proxy.dll",
        "server.exe",
        "VERSION.txt",
        "CoopAndreas_SAMP_INFO.txt",
        "Uruchom CoopAndreas.bat",
        "Uruchom CoopAndreas Server.bat",
        "Odinstaluj CoopAndreas.bat",
        "Wyczysc CoopAndreas.bat",
        "Przelacz na CoopAndreas.bat",
        "Przelacz na SA-MP.bat",
        "Aktualizuj CoopAndreas.bat",
        "Aktualizuj i Uruchom CoopAndreas.bat",
        "CoopAndreasUpdater.ps1"
    )

    foreach ($relPath in $filesToRemove) {
        Remove-FileIfExists -path (Join-Path $gtaDir $relPath)
    }

    Remove-FileIfExists -path (Join-Path $gtaDir "scripts\\GTASA.WidescreenFix.asi")
    Remove-FileIfExists -path (Join-Path $gtaDir "scripts\\GTASA.WidescreenFix.ini")

    $backupDirs = Get-ChildItem -Path $gtaDir -Directory -ErrorAction SilentlyContinue | Where-Object { $_.Name -like "CoopAndreas_backup_*" }
    foreach ($dir in $backupDirs) {
        Remove-Item -LiteralPath $dir.FullName -Recurse -Force -ErrorAction SilentlyContinue
        Write-Ok "Usunieto backup: $($dir.Name)"
    }

    Write-Ok "Odinstalowanie zakonczone. Instalacja GTA jest gotowa do testu na swiezo."
    Write-Info "Jesli chcesz, uruchom teraz 'Aktualizuj CoopAndreas.bat' aby zainstalowac wszystko od zera."
    exit 0
}
catch {
    Write-Err $_.Exception.Message
    exit 1
}
