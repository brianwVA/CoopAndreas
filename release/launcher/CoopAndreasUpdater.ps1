param(
    [string]$RepoOwner = "brianwVA",
    [string]$RepoName = "CoopAndreas",
    [string]$Branch = "old-0.2.2",
    [string]$PackagePath = "",
    [switch]$CloseRunningProcesses = $true,
    [switch]$AllowSamp,
    [switch]$RunAfterUpdate
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$tempRoot = Join-Path $env:TEMP ("coopandreas_updater_" + [guid]::NewGuid().ToString("N"))
$zipPath = Join-Path $tempRoot "repo.zip"

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

    throw "Nie znaleziono gta_sa.exe. Umiesc launcher w katalogu GTA (lub podkatalogu) i uruchom ponownie."
}

function Test-GtaSanAndreasInstall([string]$gameDir) {
    $gtaExe = Join-Path $gameDir "gta_sa.exe"
    if (-not (Test-Path $gtaExe)) {
        throw "Brak gta_sa.exe. Skopiuj launcher do glownego folderu GTA San Andreas i uruchom ponownie."
    }

    $ver = (Get-Item $gtaExe).VersionInfo.FileVersion
    if (-not [string]::IsNullOrWhiteSpace($ver) -and $ver -ne "1.0.0.0") {
        Write-Info "Wykryta wersja gta_sa.exe: $ver (zalecana: 1.0.0.0 US)."
    }
}

function Test-SampInstalled([string]$gameDir) {
    $sampMarkers = @(
        "samp.dll",
        "samp.exe",
        "samp.saa",
        "SAMP\\samp.dll",
        "SAMP\\samp.exe"
    )

    foreach ($m in $sampMarkers) {
        if (Test-Path (Join-Path $gameDir $m)) {
            return $true
        }
    }
    return $false
}

function Get-SampPaths([string]$gameDir) {
    return @(
        (Join-Path $gameDir "samp.dll"),
        (Join-Path $gameDir "samp.exe"),
        (Join-Path $gameDir "samp.saa"),
        (Join-Path $gameDir "SAMP\\samp.dll"),
        (Join-Path $gameDir "SAMP\\samp.exe")
    )
}

function Write-SampInstructions([string]$gameDir) {
    $txtPath = Join-Path $gameDir "CoopAndreas_SAMP_INFO.txt"
    $content = @"
Wykryto SA:MP w tej instalacji GTA.
SA:MP i CoopAndreas moga konfliktowac, dlatego updater blokuje update domyslnie.

Masz 3 opcje:
1) Najbezpieczniej: osobna kopia GTA dla CoopAndreas.
2) Przelacz na CoopAndreas (tymczasowo wylacz SA:MP):
   - uruchom: Przelacz na CoopAndreas.bat
   - potem uruchom updater.
3) Przywroc SA:MP:
   - uruchom: Przelacz na SA-MP.bat

Co robi przelacznik:
- zmienia nazwy plikow SA:MP (np. samp.dll -> samp.dll.coop_disabled),
- przy przywroceniu cofa nazwy.

Uwaga:
- zawsze zamknij GTA i server.exe przed przelaczaniem,
- dziala w folderze gry: $gameDir
"@
    Set-Content -LiteralPath $txtPath -Value $content -Encoding UTF8
    return $txtPath
}

function Write-SampSwitchScripts([string]$gameDir) {
    $toCoopBat = Join-Path $gameDir "Przelacz na CoopAndreas.bat"
    $toSampBat = Join-Path $gameDir "Przelacz na SA-MP.bat"

    $toCoopContent = @"
@echo off
setlocal
set "GDIR=%~dp0"
echo [INFO] Wylaczanie SA:MP dla CoopAndreas...
for %%F in ("samp.dll" "samp.exe" "samp.saa" "SAMP\samp.dll" "SAMP\samp.exe") do (
  if exist "%GDIR%%%~F" (
    if not exist "%GDIR%%%~F.coop_disabled" (
      ren "%GDIR%%%~F" "%%~nxF.coop_disabled"
      echo [OK] Wylaczono %%~F
    )
  )
)
echo [OK] Gotowe.
pause
exit /b 0
"@

    $toSampContent = @"
@echo off
setlocal
set "GDIR=%~dp0"
echo [INFO] Przywracanie SA:MP...
for %%F in ("samp.dll" "samp.exe" "samp.saa" "SAMP\samp.dll" "SAMP\samp.exe") do (
  if exist "%GDIR%%%~F.coop_disabled" (
    ren "%GDIR%%%~F.coop_disabled" "%%~nxF"
    echo [OK] Przywrocono %%~F
  )
)
echo [OK] Gotowe.
pause
exit /b 0
"@

    Set-Content -LiteralPath $toCoopBat -Value $toCoopContent -Encoding ASCII
    Set-Content -LiteralPath $toSampBat -Value $toSampContent -Encoding ASCII
}

function Disable-SampFiles([string]$gameDir) {
    $changed = $false
    foreach ($path in (Get-SampPaths -gameDir $gameDir)) {
        if (Test-Path $path) {
            $target = "$path.coop_disabled"
            if (-not (Test-Path $target)) {
                Move-Item -LiteralPath $path -Destination $target -Force
                $changed = $true
                Write-Ok "Wylaczono SA:MP plik: $(Split-Path -Leaf $path)"
            }
        }
    }
    return $changed
}

function Get-LatestPackagePath([string]$repoRootPath) {
    $releaseDir = Join-Path $repoRootPath "release"
    if (-not (Test-Path $releaseDir)) {
        throw "Brak katalogu 'release' w pobranej paczce."
    }

    $dirs = Get-ChildItem -Path $releaseDir -Directory | Where-Object { $_.Name -like "old-*" }
    if (-not $dirs -or $dirs.Count -eq 0) {
        throw "Nie znaleziono katalogow paczek (np. old-0.2.x) w 'release'."
    }

    $latest = $dirs |
        Sort-Object -Descending -Property @{
            Expression = {
                $name = $_.Name
                $versionPart = $name -replace '^old-', ''
                if ($versionPart -match '^\d+(\.\d+){1,3}$') {
                    return [version]$versionPart
                }
                return [version]'0.0.0.0'
            }
        } |
        Select-Object -First 1
    return (Join-Path "release" $latest.Name)
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

function Ensure-AsiLoader([string]$pkgDir, [string]$gameDir) {
    $loaderDst = Join-Path $gameDir "dinput8.dll"
    if (Test-Path $loaderDst) {
        Write-Ok "ASI Loader wykryty (dinput8.dll)."
        return
    }

    $loaderCandidates = @(
        (Join-Path $pkgDir "dinput8.dll"),
        (Join-Path $pkgDir "asi\\dinput8.dll"),
        (Join-Path $pkgDir "loader\\dinput8.dll")
    )

    $loaderSrc = $loaderCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
    if ($loaderSrc) {
        Copy-Item -LiteralPath $loaderSrc -Destination $loaderDst -Force
        Write-Ok "Zainstalowano ASI Loader (dinput8.dll)."
        return
    }

    throw "Brak dinput8.dll (ASI Loader). Zainstaluj Ultimate ASI Loader albo dodaj dinput8.dll do paczki release."
}

try {
    $gtaDir = Resolve-GtaDir -startDir $scriptDir
    Test-GtaSanAndreasInstall -gameDir $gtaDir

    Write-SampSwitchScripts -gameDir $gtaDir
    $sampInfoPath = Write-SampInstructions -gameDir $gtaDir

    $sampDetected = Test-SampInstalled -gameDir $gtaDir
    if ((-not $AllowSamp) -and $sampDetected) {
        Write-Err "Wykryto SA:MP - moze byc niekompatybilny z CoopAndreas."
        Write-Info "Instrukcja zapisana do: $sampInfoPath"
        Write-Host ""
        Write-Host "Wybierz opcje:" -ForegroundColor Yellow
        Write-Host "1) Anuluj (bezpiecznie, domyslnie)"
        Write-Host "2) Wylacz SA:MP i kontynuuj update (zalecane jesli 1 folder gry)"
        Write-Host "3) Kontynuuj bez zmian (ryzykowne)"
        $choice = Read-Host "Twoj wybor [1/2/3]"
        if ([string]::IsNullOrWhiteSpace($choice)) { $choice = "1" }

        switch ($choice) {
            "2" {
                $changed = Disable-SampFiles -gameDir $gtaDir
                if ($changed) {
                    Write-Ok "SA:MP tymczasowo wylaczony. Po grze przywroc przez 'Przelacz na SA-MP.bat'."
                } else {
                    Write-Info "Nie znaleziono plikow do wylaczenia (mogly byc juz wylaczone)."
                }
            }
            "3" {
                Write-Info "Kontynuacja bez wylaczania SA:MP na Twoja odpowiedzialnosc."
            }
            default {
                throw "Aktualizacja przerwana przez uzytkownika po wykryciu SA:MP."
            }
        }
    }

    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    $zipUrl = "https://codeload.github.com/$RepoOwner/$RepoName/zip/refs/heads/$Branch"
    Write-Info "Pobieram paczke: $zipUrl"
    Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath -UseBasicParsing

    Write-Info "Rozpakowuje paczke..."
    Expand-Archive -Path $zipPath -DestinationPath $tempRoot -Force

    $repoRoot = Get-ChildItem -Path $tempRoot -Directory | Where-Object { $_.Name -like "$RepoName-*" } | Select-Object -First 1
    if (-not $repoRoot) { throw "Nie znaleziono katalogu repo po rozpakowaniu." }

    if ([string]::IsNullOrWhiteSpace($PackagePath)) {
        $PackagePath = Get-LatestPackagePath -repoRootPath $repoRoot.FullName
        Write-Info "Wybrano najnowsza paczke: $PackagePath"
    }

    $pkg = Join-Path $repoRoot.FullName ($PackagePath -replace '/', '\')
    if (-not (Test-Path $pkg)) {
        throw "Brak folderu paczki '$PackagePath' w branchu '$Branch'."
    }

    $files = @("CoopAndreasSA.dll", "server.exe", "proxy.dll", "VERSION.txt")

    if ($CloseRunningProcesses) {
        Stop-IfRunning -processNames @("server", "gta_sa")
    }

    Ensure-AsiLoader -pkgDir $pkg -gameDir $gtaDir

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
