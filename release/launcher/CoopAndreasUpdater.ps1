param(
    [string]$RepoOwner = "brianwVA",
    [string]$RepoName = "CoopAndreas",
    [string]$Branch = "old-0.2.2",
    [string]$PackagePath = "",
    [switch]$CloseRunningProcesses = $true,
    [switch]$AllowSamp,
    [switch]$RunAfterUpdate,
    [switch]$NoResolutionFix
)

$ErrorActionPreference = "Stop"
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$tempRoot = Join-Path $env:TEMP ("coopandreas_updater_" + [guid]::NewGuid().ToString("N"))
$zipPath = Join-Path $tempRoot "repo.zip"

function Write-Info([string]$m) { Write-Host "[INFO] $m" -ForegroundColor Cyan }
function Write-Ok([string]$m) { Write-Host "[OK]   $m" -ForegroundColor Green }
function Write-Err([string]$m) { Write-Host "[ERR]  $m" -ForegroundColor Red }

function Get-GitHubHeaders() {
    return @{
        "User-Agent" = "CoopAndreas-Updater"
        "Accept" = "application/vnd.github+json"
    }
}

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

function Get-LocalInstalledChannel([string]$gameDir) {
    $versionPath = Join-Path $gameDir "VERSION.txt"
    if (-not (Test-Path $versionPath)) {
        return $null
    }

    $line = Get-Content -LiteralPath $versionPath | Where-Object { $_ -like "channel=*" } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($line)) {
        return $null
    }

    return ($line -replace '^channel=', '').Trim()
}

function Get-LatestRemotePackageName([string]$owner, [string]$repo, [string]$branch) {
    $apiUrl = "https://api.github.com/repos/$owner/$repo/contents/release?ref=$branch"
    $headers = Get-GitHubHeaders

    $items = Invoke-RestMethod -Uri $apiUrl -Headers $headers -Method Get
    $dirs = @($items | Where-Object { $_.type -eq "dir" -and $_.name -like "old-*" })
    if (-not $dirs -or $dirs.Count -eq 0) {
        throw "Nie znaleziono katalogow old-* przez GitHub API."
    }

    $latest = $dirs |
        Sort-Object -Descending -Property @{
            Expression = {
                $versionPart = ($_.name -replace '^old-', '')
                if ($versionPart -match '^\d+(\.\d+){1,3}$') {
                    return [version]$versionPart
                }
                return [version]'0.0.0.0'
            }
        } |
        Select-Object -First 1

    return $latest.name
}

function Get-GitHubReleaseAssetUrl(
    [string]$owner,
    [string]$repo,
    [string]$tag,
    [string[]]$namePatterns
) {
    $headers = Get-GitHubHeaders
    $releaseUrl = if ([string]::IsNullOrWhiteSpace($tag)) {
        "https://api.github.com/repos/$owner/$repo/releases/latest"
    } else {
        "https://api.github.com/repos/$owner/$repo/releases/tags/$tag"
    }

    try {
        $release = Invoke-RestMethod -Uri $releaseUrl -Headers $headers -Method Get
    }
    catch {
        return $null
    }

    if (-not $release.assets) {
        return $null
    }

    foreach ($pattern in $namePatterns) {
        $asset = $release.assets | Where-Object { $_.name -like $pattern } | Select-Object -First 1
        if ($asset -and -not [string]::IsNullOrWhiteSpace($asset.browser_download_url)) {
            return $asset.browser_download_url
        }
    }

    return $null
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

    Write-Info "Brak lokalnego dinput8.dll - probuje pobrac Ultimate ASI Loader..."
    $asiUrl = Get-GitHubReleaseAssetUrl -owner "ThirteenAG" -repo "Ultimate-ASI-Loader" -tag "" -namePatterns @("*x86*.zip", "*.zip")
    if ([string]::IsNullOrWhiteSpace($asiUrl)) {
        throw "Brak dinput8.dll i nie udalo sie pobrac Ultimate ASI Loader z GitHub."
    }

    $asiTemp = Join-Path $tempRoot "asi_loader"
    $asiZip = Join-Path $asiTemp "asi_loader.zip"
    New-Item -ItemType Directory -Path $asiTemp -Force | Out-Null

    Invoke-WebRequest -Uri $asiUrl -OutFile $asiZip -UseBasicParsing
    Expand-Archive -Path $asiZip -DestinationPath $asiTemp -Force

    $dllFiles = Get-ChildItem -Path $asiTemp -Recurse -File -Filter "dinput8.dll"
    if (-not $dllFiles -or $dllFiles.Count -eq 0) {
        throw "Pobrano Ultimate ASI Loader, ale nie znaleziono dinput8.dll."
    }

    $bestDll = $dllFiles |
        Sort-Object -Property @{
            Expression = {
                if ($_.FullName -match '(?i)(\\|/)(x86|win32)(\\|/)') { return 0 }
                return 1
            }
        } |
        Select-Object -First 1

    Copy-Item -LiteralPath $bestDll.FullName -Destination $loaderDst -Force
    Write-Ok "Pobrano i zainstalowano ASI Loader (dinput8.dll)."
}

function Ensure-WidescreenFix([string]$gameDir) {
    $scriptsDir = Join-Path $gameDir "scripts"
    if (-not (Test-Path $scriptsDir)) {
        New-Item -ItemType Directory -Path $scriptsDir -Force | Out-Null
    }

    $asiDst = Join-Path $scriptsDir "GTASA.WidescreenFix.asi"
    $iniDst = Join-Path $scriptsDir "GTASA.WidescreenFix.ini"
    if (Test-Path $asiDst) {
        Write-Ok "WidescreenFix juz zainstalowany."
        return
    }

    Write-Info "Brak WidescreenFix - probuje pobrac z GitHub..."
    $wsUrl = Get-GitHubReleaseAssetUrl -owner "ThirteenAG" -repo "WidescreenFixesPack" -tag "gtasa" -namePatterns @("GTASA.WidescreenFix*.zip", "*.zip")
    if ([string]::IsNullOrWhiteSpace($wsUrl)) {
        $wsUrl = Get-GitHubReleaseAssetUrl -owner "ThirteenAG" -repo "WidescreenFixesPack" -tag "" -namePatterns @("GTASA.WidescreenFix*.zip", "*.zip")
    }
    if ([string]::IsNullOrWhiteSpace($wsUrl)) {
        Write-Info "Nie udalo sie pobrac WidescreenFix automatycznie. Pomijam fix rozdzielczosci."
        return
    }

    $wsTemp = Join-Path $tempRoot "widescreen_fix"
    $wsZip = Join-Path $wsTemp "widescreen_fix.zip"
    New-Item -ItemType Directory -Path $wsTemp -Force | Out-Null

    Invoke-WebRequest -Uri $wsUrl -OutFile $wsZip -UseBasicParsing
    Expand-Archive -Path $wsZip -DestinationPath $wsTemp -Force

    $asiSrc = Get-ChildItem -Path $wsTemp -Recurse -File -Filter "GTASA.WidescreenFix.asi" | Select-Object -First 1
    if (-not $asiSrc) {
        Write-Info "Pobrano paczke WidescreenFix, ale brak GTASA.WidescreenFix.asi. Pomijam."
        return
    }

    Copy-Item -LiteralPath $asiSrc.FullName -Destination $asiDst -Force
    Write-Ok "Zainstalowano scripts\\GTASA.WidescreenFix.asi"

    $iniSrc = Get-ChildItem -Path $wsTemp -Recurse -File -Filter "GTASA.WidescreenFix.ini" | Select-Object -First 1
    if ($iniSrc) {
        Copy-Item -LiteralPath $iniSrc.FullName -Destination $iniDst -Force
        Write-Ok "Zainstalowano scripts\\GTASA.WidescreenFix.ini"
    }
}

function Write-LocalLaunchers([string]$gameDir) {
    $runServerBat = Join-Path $gameDir "Uruchom CoopAndreas Server.bat"
    $runCoopBat = Join-Path $gameDir "Uruchom CoopAndreas.bat"
    $cleanBat = Join-Path $gameDir "Odinstaluj CoopAndreas.bat"

    $serverBatContent = @"
@echo off
setlocal
set "GTA_DIR=%~dp0"
set "SERVER_EXE=%GTA_DIR%server.exe"

if not exist "%SERVER_EXE%" (
  echo [BLAD] Nie znaleziono pliku: "%SERVER_EXE%"
  pause
  exit /b 1
)

set "PID="
for /f "tokens=5" %%A in ('netstat -ano -p udp ^| findstr /r /c:":6767[ ]"') do (
  set "PID=%%A"
  goto :port_in_use
)

cd /d "%GTA_DIR%"
echo [INFO] Uruchamiam lokalny serwer CoopAndreas...
start "CoopAndreas Server" cmd /k ""%SERVER_EXE%""
exit /b 0

:port_in_use
echo [INFO] Port 6767 jest juz zajety (PID: %PID%).
echo [INFO] Najpewniej serwer CoopAndreas juz dziala.
pause
exit /b 0
"@

    $runCoopContent = @"
@echo off
setlocal
call "%~dp0Aktualizuj i Uruchom CoopAndreas.bat"
exit /b %errorlevel%
"@

    $cleanContent = @"
@echo off
setlocal
set "PS_EXE=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
"%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0CoopAndreasCleaner.ps1"
if errorlevel 1 (
  echo.
  echo [BLAD] Odinstalowanie nieudane.
  pause
  exit /b 1
)
echo.
echo [OK] Odinstalowanie zakonczone.
pause
exit /b 0
"@

    Set-Content -LiteralPath $runServerBat -Value $serverBatContent -Encoding ASCII
    Set-Content -LiteralPath $runCoopBat -Value $runCoopContent -Encoding ASCII
    Set-Content -LiteralPath $cleanBat -Value $cleanContent -Encoding ASCII
    Write-Ok "Odswiezono launchery BAT w folderze GTA."
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

    $localChannelRaw = Get-LocalInstalledChannel -gameDir $gtaDir
    $localChannelDisplay = $localChannelRaw
    if ([string]::IsNullOrWhiteSpace($localChannelDisplay)) {
        $localChannelDisplay = "brak/nieznana"
    }

    $remotePackageName = $null
    try {
        $remotePackageName = Get-LatestRemotePackageName -owner $RepoOwner -repo $RepoName -branch $Branch
        Write-Info "Zainstalowana wersja: $localChannelDisplay"
        Write-Info "Najnowsza wersja na GitHub: $remotePackageName"

        if (-not [string]::IsNullOrWhiteSpace($localChannelRaw) -and $localChannelRaw -eq $remotePackageName) {
            Write-Ok "Masz juz najnowsza wersje: $remotePackageName"
            Write-Info "Pomijam pobieranie paczki ZIP (brak zmian)."
            Write-LocalLaunchers -gameDir $gtaDir

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

            Write-Ok "Brak aktualizacji - wersja juz najnowsza."
            exit 0
        } elseif (-not [string]::IsNullOrWhiteSpace($remotePackageName)) {
            Write-Info "Wykryto nowa wersje: $remotePackageName (aktualna lokalna: $localChannelDisplay)"
        }
    }
    catch {
        Write-Info "Nie udalo sie sprawdzic wersji przez GitHub API. Przechodze do standardowej aktualizacji."
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
        if (-not [string]::IsNullOrWhiteSpace($remotePackageName)) {
            $PackagePath = "release/$remotePackageName"
        } else {
            $PackagePath = Get-LatestPackagePath -repoRootPath $repoRoot.FullName
        }
        Write-Info "Wybrano najnowsza paczke: $PackagePath"
    }

    $pkg = Join-Path $repoRoot.FullName ($PackagePath -replace '/', '\')
    if (-not (Test-Path $pkg)) {
        throw "Brak folderu paczki '$PackagePath' w branchu '$Branch'."
    }

    $remoteChannel = Split-Path -Leaf ($PackagePath -replace '/', '\')
    $needsUpdate = ($localChannelRaw -ne $remoteChannel)

    $files = @("CoopAndreasSA.dll", "server.exe", "proxy.dll", "VERSION.txt")

    if ($CloseRunningProcesses) {
        Stop-IfRunning -processNames @("server", "gta_sa")
    }

    Ensure-AsiLoader -pkgDir $pkg -gameDir $gtaDir
    if (-not $NoResolutionFix) {
        Ensure-WidescreenFix -gameDir $gtaDir
    } else {
        Write-Info "Pominieto fix rozdzielczosci na prosbe uzytkownika (-NoResolutionFix)."
    }

    if ($needsUpdate) {
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
    } else {
        Write-Info "Pomijam podmiane plikow, bo wersja jest juz aktualna."
    }

    Write-LocalLaunchers -gameDir $gtaDir

    if ($needsUpdate) {
        Write-Ok "Aktualizacja zakonczona pomyslnie."
    } else {
        Write-Ok "Brak aktualizacji - wersja juz najnowsza."
    }

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
