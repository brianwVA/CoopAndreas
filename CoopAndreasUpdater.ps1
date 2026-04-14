param(
    [string]$RepoOwner = "brianwVA",
    [string]$RepoName = "CoopAndreas",
    [string]$Branch = "main",
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
        if ([string]::IsNullOrWhiteSpace($parent) -or $parent -eq $current) { break }
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
    $sampMarkers = @("samp.dll", "samp.exe", "samp.saa", "SAMP\samp.dll", "SAMP\samp.exe")
    foreach ($m in $sampMarkers) {
        if (Test-Path (Join-Path $gameDir $m)) { return $true }
    }
    return $false
}

function Get-SampPaths([string]$gameDir) {
    return @(
        (Join-Path $gameDir "samp.dll"),
        (Join-Path $gameDir "samp.exe"),
        (Join-Path $gameDir "samp.saa"),
        (Join-Path $gameDir "SAMP\samp.dll"),
        (Join-Path $gameDir "SAMP\samp.exe")
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

# ── Integrity check ──

function Test-CoreFilesPresent([string]$gameDir) {
    $requiredFiles = @("CoopAndreasSA.dll", "server.exe", "EAX.dll", "stream.ini")
    $missing = @()
    foreach ($f in $requiredFiles) {
        if (-not (Test-Path (Join-Path $gameDir $f))) {
            $missing += $f
        }
    }
    # Check CoopAndreas\main.scm
    $scmPath = Join-Path $gameDir "CoopAndreas\main.scm"
    if (-not (Test-Path $scmPath)) { $missing += "CoopAndreas\main.scm" }
    return $missing
}

# ── MSVC Runtime check ──

function Test-VCRedistInstalled() {
    # Check for VC++ 2015-2022 x86 redistributable
    $paths = @(
        "HKLM:\SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\X86",
        "HKLM:\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X86"
    )
    foreach ($p in $paths) {
        if (Test-Path $p) {
            $installed = (Get-ItemProperty -Path $p -ErrorAction SilentlyContinue).Installed
            if ($installed -eq 1) { return $true }
        }
    }
    # Fallback: check if vcruntime140.dll exists in System32 or SysWOW64
    $sysDir = if ([Environment]::Is64BitOperatingSystem) { "$env:SystemRoot\SysWOW64" } else { "$env:SystemRoot\System32" }
    return (Test-Path (Join-Path $sysDir "vcruntime140.dll"))
}

function Install-VCRedist() {
    $vcUrl = "https://aka.ms/vs/17/release/vc_redist.x86.exe"
    $vcTemp = Join-Path $env:TEMP "vc_redist_x86.exe"
    Write-Info "Pobieram Visual C++ Redistributable x86..."
    try {
        Invoke-WebRequest -Uri $vcUrl -OutFile $vcTemp -UseBasicParsing
        Write-Info "Instalowanie Visual C++ Redistributable (wymaga uprawnien administratora)..."
        $proc = Start-Process -FilePath $vcTemp -ArgumentList "/install", "/passive", "/norestart" -Wait -PassThru
        if ($proc.ExitCode -eq 0 -or $proc.ExitCode -eq 3010) {
            Write-Ok "Visual C++ Redistributable zainstalowany."
        } else {
            Write-Err "Instalacja VC++ Redist zwrocila kod: $($proc.ExitCode)"
            Write-Info "Pobierz recznie: https://aka.ms/vs/17/release/vc_redist.x86.exe"
        }
    } catch {
        Write-Err "Nie udalo sie pobrac/zainstalowac VC++ Redistributable."
        Write-Info "Pobierz recznie: https://aka.ms/vs/17/release/vc_redist.x86.exe"
    } finally {
        if (Test-Path $vcTemp) { Remove-Item $vcTemp -Force -ErrorAction SilentlyContinue }
    }
}

# ── DirectX 9 Runtime ──

function Test-DirectX9Installed() {
    # Check for d3dx9_43.dll (required by CoopAndreasSA.dll)
    $sysDir = if ([Environment]::Is64BitOperatingSystem) { "$env:SystemRoot\SysWOW64" } else { "$env:SystemRoot\System32" }
    return (Test-Path (Join-Path $sysDir "d3dx9_43.dll"))
}

function Install-DirectX9() {
    $dxUrl = "https://download.microsoft.com/download/8/4/A/84A35BF1-DAFE-4AE8-82AF-AD2AE20B6B14/directx_Jun2010_redist.exe"
    $dxTemp = Join-Path $env:TEMP "directx_redist.exe"
    $dxExtract = Join-Path $env:TEMP "directx_extract"
    Write-Info "Pobieram DirectX 9 End-User Runtime (~100MB)..."
    try {
        Invoke-WebRequest -Uri $dxUrl -OutFile $dxTemp -UseBasicParsing
        Write-Info "Rozpakowuje DirectX Runtime..."
        New-Item -ItemType Directory -Path $dxExtract -Force | Out-Null
        $proc = Start-Process -FilePath $dxTemp -ArgumentList "/Q", "/T:$dxExtract" -Wait -PassThru
        if ($proc.ExitCode -ne 0) {
            Write-Err "Nie udalo sie rozpakowac DirectX Runtime."
            Write-Info "Pobierz recznie: https://www.microsoft.com/en-us/download/details.aspx?id=8109"
            return
        }
        $dxSetup = Join-Path $dxExtract "DXSETUP.exe"
        if (Test-Path $dxSetup) {
            Write-Info "Instalowanie DirectX Runtime (wymaga uprawnien administratora)..."
            $proc2 = Start-Process -FilePath $dxSetup -ArgumentList "/silent" -Wait -PassThru
            if ($proc2.ExitCode -eq 0) {
                Write-Ok "DirectX 9 Runtime zainstalowany."
            } else {
                Write-Err "Instalacja DirectX Runtime zwrocila kod: $($proc2.ExitCode)"
                Write-Info "Pobierz recznie: https://www.microsoft.com/en-us/download/details.aspx?id=8109"
            }
        }
    } catch {
        Write-Err "Nie udalo sie pobrac/zainstalowac DirectX 9 Runtime."
        Write-Info "Pobierz recznie: https://www.microsoft.com/en-us/download/details.aspx?id=8109"
    } finally {
        if (Test-Path $dxTemp) { Remove-Item $dxTemp -Force -ErrorAction SilentlyContinue }
        if (Test-Path $dxExtract) { Remove-Item $dxExtract -Recurse -Force -ErrorAction SilentlyContinue }
    }
}

# ── Unblock downloaded files ──

function Unblock-ModFiles([string]$gameDir) {
    $filesToUnblock = @("CoopAndreasSA.dll", "EAX.dll", "proxy.dll", "dinput8.dll", "server.exe")
    foreach ($f in $filesToUnblock) {
        $path = Join-Path $gameDir $f
        if (Test-Path $path) {
            Unblock-File -LiteralPath $path -ErrorAction SilentlyContinue
        }
    }
}

# ── Commit-based update check ──

function Get-LocalCommitSha([string]$gameDir) {
    $versionPath = Join-Path $gameDir "VERSION.txt"
    if (-not (Test-Path $versionPath)) { return $null }
    $line = Get-Content -LiteralPath $versionPath | Where-Object { $_ -like "commit=*" } | Select-Object -First 1
    if ([string]::IsNullOrWhiteSpace($line)) { return $null }
    return ($line -replace '^commit=', '').Trim()
}

function Get-RemoteCommitSha([string]$owner, [string]$repo, [string]$branch) {
    $apiUrl = "https://api.github.com/repos/$owner/$repo/commits/$branch"
    $headers = Get-GitHubHeaders
    $response = Invoke-RestMethod -Uri $apiUrl -Headers $headers -Method Get
    return $response.sha
}

# ── ASI Loader ──

function Get-GitHubReleaseAssetUrl([string]$owner, [string]$repo, [string]$tag, [string[]]$namePatterns) {
    $headers = Get-GitHubHeaders
    $releaseUrl = if ([string]::IsNullOrWhiteSpace($tag)) {
        "https://api.github.com/repos/$owner/$repo/releases/latest"
    } else {
        "https://api.github.com/repos/$owner/$repo/releases/tags/$tag"
    }
    try {
        $release = Invoke-RestMethod -Uri $releaseUrl -Headers $headers -Method Get
    } catch { return $null }
    if (-not $release.assets) { return $null }
    foreach ($pattern in $namePatterns) {
        $asset = $release.assets | Where-Object { $_.name -like $pattern } | Select-Object -First 1
        if ($asset -and -not [string]::IsNullOrWhiteSpace($asset.browser_download_url)) {
            return $asset.browser_download_url
        }
    }
    return $null
}

function Ensure-AsiLoader([string]$gameDir) {
    $loaderDst = Join-Path $gameDir "dinput8.dll"
    if (Test-Path $loaderDst) {
        Write-Ok "ASI Loader wykryty (dinput8.dll)."
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

# ── Widescreen Fix ──

function Ensure-WidescreenFix([string]$gameDir) {
    $scriptsDir = Join-Path $gameDir "scripts"
    if (-not (Test-Path $scriptsDir)) {
        New-Item -ItemType Directory -Path $scriptsDir -Force | Out-Null
    }

    $asiDst = Join-Path $scriptsDir "GTASA.WidescreenFix.asi"
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
        Write-Info "Nie udalo sie pobrac WidescreenFix automatycznie. Pomijam."
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
    Write-Ok "Zainstalowano scripts\GTASA.WidescreenFix.asi"

    $iniSrc = Get-ChildItem -Path $wsTemp -Recurse -File -Filter "GTASA.WidescreenFix.ini" | Select-Object -First 1
    if ($iniSrc) {
        $iniDst = Join-Path $scriptsDir "GTASA.WidescreenFix.ini"
        Copy-Item -LiteralPath $iniSrc.FullName -Destination $iniDst -Force
        Write-Ok "Zainstalowano scripts\GTASA.WidescreenFix.ini"
    }
}

# ── Proxy / EAX setup ──

function Ensure-ProxySetup([string]$repoRoot, [string]$gameDir) {
    $eaxOrig = Join-Path $gameDir "eax_orig.dll"
    $eaxDll = Join-Path $gameDir "EAX.dll"

    # Rename original eax.dll to eax_orig.dll if not done yet
    if (-not (Test-Path $eaxOrig)) {
        if (Test-Path $eaxDll) {
            # Only rename if current EAX.dll is NOT our proxy (check size — proxy is ~85KB, original is ~188KB)
            $eaxSize = (Get-Item $eaxDll).Length
            if ($eaxSize -gt 100000) {
                Move-Item -LiteralPath $eaxDll -Destination $eaxOrig -Force
                Write-Ok "Zmieniono nazwe oryginalnego eax.dll -> eax_orig.dll"
            }
        }
    }

    # Copy proxy.dll as EAX.dll
    $proxySrc = Join-Path $repoRoot "release"
    $latestPkg = Get-ChildItem -Path $proxySrc -Directory | Where-Object { $_.Name -like "old-*" } |
        Sort-Object -Descending -Property @{
            Expression = {
                $vp = ($_.Name -replace '^old-', '')
                if ($vp -match '^\d+(\.\d+){1,3}$') { return [version]$vp }
                return [version]'0.0.0.0'
            }
        } | Select-Object -First 1

    if ($latestPkg) {
        $proxyFile = Join-Path $latestPkg.FullName "proxy.dll"
        if (Test-Path $proxyFile) {
            Copy-Item -LiteralPath $proxyFile -Destination $eaxDll -Force
            Write-Ok "Zainstalowano proxy.dll jako EAX.dll (loader DLL)"
            # Also copy proxy.dll to root
            Copy-Item -LiteralPath $proxyFile -Destination (Join-Path $gameDir "proxy.dll") -Force
        }
    }
}

# ── SilentPatch ──

function Ensure-SilentPatch([string]$repoRoot, [string]$gameDir) {
    $asiDst = Join-Path $gameDir "SilentPatchSA.asi"
    $iniDst = Join-Path $gameDir "SilentPatchSA.ini"

    # Already installed? Don't re-download
    if (Test-Path $asiDst) {
        Write-Ok "SilentPatchSA juz zainstalowany."
        return
    }

    Write-Info "Brak SilentPatchSA - probuje pobrac z GitHub..."
    $spUrl = Get-GitHubReleaseAssetUrl -owner "CookiePLMonster" -repo "SilentPatchSA" -tag "" -namePatterns @("SilentPatchSA*.zip", "*.zip")
    if ([string]::IsNullOrWhiteSpace($spUrl)) {
        Write-Info "Nie udalo sie pobrac SilentPatchSA. Pomijam."
        return
    }

    $spTemp = Join-Path $tempRoot "silentpatch"
    $spZip = Join-Path $spTemp "silentpatch.zip"
    New-Item -ItemType Directory -Path $spTemp -Force | Out-Null
    Invoke-WebRequest -Uri $spUrl -OutFile $spZip -UseBasicParsing
    Expand-Archive -Path $spZip -DestinationPath $spTemp -Force

    $foundAsi = Get-ChildItem -Path $spTemp -Recurse -File -Filter "SilentPatchSA.asi" | Select-Object -First 1
    if ($foundAsi) {
        Copy-Item -LiteralPath $foundAsi.FullName -Destination $asiDst -Force
        Write-Ok "Pobrano i zainstalowano SilentPatchSA.asi"
    }

    $foundIni = Get-ChildItem -Path $spTemp -Recurse -File -Filter "SilentPatchSA.ini" | Select-Object -First 1
    if ($foundIni) {
        Copy-Item -LiteralPath $foundIni.FullName -Destination $iniDst -Force
        Write-Ok "Pobrano i zainstalowano SilentPatchSA.ini"
    }
}

# ── Full mod install ──

function Install-FullMod([string]$repoRoot, [string]$gameDir) {
    Write-Info "Instalacja pelnego moda CoopAndreas..."

    # 1. Find latest release package
    $releaseDir = Join-Path $repoRoot "release"
    $latestPkg = Get-ChildItem -Path $releaseDir -Directory | Where-Object { $_.Name -like "old-*" } |
        Sort-Object -Descending -Property @{
            Expression = {
                $vp = ($_.Name -replace '^old-', '')
                if ($vp -match '^\d+(\.\d+){1,3}$') { return [version]$vp }
                return [version]'0.0.0.0'
            }
        } | Select-Object -First 1

    if (-not $latestPkg) {
        throw "Nie znaleziono paczki release w repo."
    }

    $pkgDir = $latestPkg.FullName
    $channelName = $latestPkg.Name
    Write-Info "Paczka: $channelName"

    # 2. Core DLL files from release package
    $coreFiles = @("CoopAndreasSA.dll", "server.exe", "proxy.dll", "VERSION.txt")
    foreach ($f in $coreFiles) {
        $src = Join-Path $pkgDir $f
        if (Test-Path $src) {
            Copy-Item -LiteralPath $src -Destination (Join-Path $gameDir $f) -Force
            Write-Ok "Zainstalowano: $f"
        }
    }

    # 3. SCM / mission scripts -> CoopAndreas/
    $coopDir = Join-Path $gameDir "CoopAndreas"
    if (-not (Test-Path $coopDir)) {
        New-Item -ItemType Directory -Path $coopDir -Force | Out-Null
    }

    $scmDir = Join-Path $repoRoot "scm"
    if (Test-Path $scmDir) {
        $scmFiles = @("main.scm", "main.txt", "script.img")
        foreach ($f in $scmFiles) {
            $src = Join-Path $scmDir $f
            if (Test-Path $src) {
                Copy-Item -LiteralPath $src -Destination (Join-Path $coopDir $f) -Force
                Write-Ok "Zainstalowano: CoopAndreas\$f"
            }
        }
    }

    # 4. Proxy -> EAX.dll setup
    Ensure-ProxySetup -repoRoot $repoRoot -gameDir $gameDir

    # 5. SilentPatch
    Ensure-SilentPatch -repoRoot $repoRoot -gameDir $gameDir

    # 6. stream.ini (always update from repo)
    $streamIniSrc = Join-Path $repoRoot "stream.ini"
    $streamIniDst = Join-Path $gameDir "stream.ini"
    if (Test-Path $streamIniSrc) {
        Copy-Item -LiteralPath $streamIniSrc -Destination $streamIniDst -Force
        Write-Ok "Zainstalowano: stream.ini"
    }

    # 7. server-config.ini (default config if not present)
    $srvCfgDst = Join-Path $gameDir "server-config.ini"
    if (-not (Test-Path $srvCfgDst)) {
        $srvCfgSrc = Join-Path $repoRoot "server-config.ini"
        if (Test-Path $srvCfgSrc) {
            Copy-Item -LiteralPath $srvCfgSrc -Destination $srvCfgDst -Force
            Write-Ok "Zainstalowano: server-config.ini"
        }
    }

    # 8. Launcher files -> CoopAndreas/Launcher/
    $launcherDir = Join-Path $coopDir "Launcher"
    if (-not (Test-Path $launcherDir)) {
        New-Item -ItemType Directory -Path $launcherDir -Force | Out-Null
    }

    # Self-update: copy the updater script from the downloaded repo
    $updaterSrc = Join-Path $repoRoot "CoopAndreasUpdater.ps1"
    if (Test-Path $updaterSrc) {
        Copy-Item -LiteralPath $updaterSrc -Destination (Join-Path $launcherDir "CoopAndreasUpdater.ps1") -Force
        Write-Ok "Zaktualizowano: CoopAndreas\Launcher\CoopAndreasUpdater.ps1"
    }

    # Copy cleaner script
    $cleanerSrc = Join-Path $repoRoot "release\launcher\CoopAndreasCleaner.ps1"
    if (Test-Path $cleanerSrc) {
        Copy-Item -LiteralPath $cleanerSrc -Destination (Join-Path $launcherDir "CoopAndreasCleaner.ps1") -Force
        Write-Ok "Zainstalowano: CoopAndreas\Launcher\CoopAndreasCleaner.ps1"
    }

    Write-Ok "Pelna instalacja moda zakonczona."
}

# ── BAT launchers ──

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
call "%~dp0CoopAndreas\Launcher\Aktualizuj i Uruchom CoopAndreas.bat"
exit /b %errorlevel%
"@

    $cleanContent = @"
@echo off
setlocal
set "PS_EXE=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
"%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%~dp0CoopAndreas\Launcher\CoopAndreasCleaner.ps1"
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

    # Also write launcher BATs inside CoopAndreas/Launcher/
    $launcherDir = Join-Path $gameDir "CoopAndreas\Launcher"
    if (-not (Test-Path $launcherDir)) {
        New-Item -ItemType Directory -Path $launcherDir -Force | Out-Null
    }

    $aktualizujBat = Join-Path $launcherDir "Aktualizuj CoopAndreas.bat"
    $aktualizujContent = @"
@echo off
setlocal
title CoopAndreas - Aktualizacja
set "SCRIPT_DIR=%~dp0"
set "PS_EXE=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
set "PS1_FILE=%SCRIPT_DIR%CoopAndreasUpdater.ps1"

if not exist "%PS1_FILE%" (
  echo [BLAD] Brak pliku CoopAndreasUpdater.ps1
  pause
  exit /b 1
)

echo [INFO] Uruchamiam CoopAndreas Updater...
"%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -Command "& { try { & '%PS1_FILE%' } catch { Write-Host ('[BLAD] ' + `$_.Exception.Message) -ForegroundColor Red; exit 1 }; exit `$LASTEXITCODE }"
set "EC=%errorlevel%"
if %EC% neq 0 (
  echo.
  echo [BLAD] Aktualizacja nieudana (kod: %EC%).
  pause
  exit /b %EC%
)

echo.
echo [OK] Aktualizacja zakonczona.
pause
exit /b 0
"@
    Set-Content -LiteralPath $aktualizujBat -Value $aktualizujContent -Encoding ASCII

    $aktualizujUruchomBat = Join-Path $launcherDir "Aktualizuj i Uruchom CoopAndreas.bat"
    $aktualizujUruchomContent = @"
@echo off
setlocal
title CoopAndreas - Aktualizuj i uruchom
set "SCRIPT_DIR=%~dp0"
set "PS_EXE=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
set "PS1_FILE=%SCRIPT_DIR%CoopAndreasUpdater.ps1"

if not exist "%PS1_FILE%" (
  echo [BLAD] Brak pliku CoopAndreasUpdater.ps1
  pause
  exit /b 1
)

echo [INFO] Uruchamiam CoopAndreas Updater...
"%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -Command "& { try { & '%PS1_FILE%' -RunAfterUpdate } catch { Write-Host ('[BLAD] ' + `$_.Exception.Message) -ForegroundColor Red; exit 1 }; exit `$LASTEXITCODE }"
set "EC=%errorlevel%"
if %EC% neq 0 (
  echo.
  echo [BLAD] Aktualizacja lub uruchomienie nieudane (kod: %EC%).
  pause
  exit /b %EC%
)

echo.
echo [OK] Aktualizacja i uruchomienie wykonane.
exit /b 0
"@
    Set-Content -LiteralPath $aktualizujUruchomBat -Value $aktualizujUruchomContent -Encoding ASCII

    $odinstalujBat = Join-Path $launcherDir "Odinstaluj CoopAndreas.bat"
    $odinstalujContent = @"
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
    Set-Content -LiteralPath $odinstalujBat -Value $odinstalujContent -Encoding ASCII

    Write-Ok "Odswiezono launchery BAT."
}

# ── Write VERSION.txt with commit SHA ──

function Write-VersionFile([string]$gameDir, [string]$commitSha, [string]$channelName) {
    $versionPath = Join-Path $gameDir "VERSION.txt"
    $content = @"
channel=$channelName
commit=$commitSha
source_branch=main
"@
    Set-Content -LiteralPath $versionPath -Value $content -Encoding UTF8
}

# ═══════════════════════════
#           MAIN
# ═══════════════════════════

try {
    Write-Host ""
    Write-Host "  ╔══════════════════════════════════════╗" -ForegroundColor Yellow
    Write-Host "  ║   CoopAndreas Installer / Updater    ║" -ForegroundColor Yellow
    Write-Host "  ╚══════════════════════════════════════╝" -ForegroundColor Yellow
    Write-Host ""

    $gtaDir = Resolve-GtaDir -startDir $scriptDir
    Write-Info "Katalog GTA: $gtaDir"
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
        Write-Host "2) Wylacz SA:MP i kontynuuj (zalecane jesli 1 folder gry)"
        Write-Host "3) Kontynuuj bez zmian (ryzykowne)"
        $choice = Read-Host "Twoj wybor [1/2/3]"
        if ([string]::IsNullOrWhiteSpace($choice)) { $choice = "1" }

        switch ($choice) {
            "2" {
                $changed = Disable-SampFiles -gameDir $gtaDir
                if ($changed) {
                    Write-Ok "SA:MP tymczasowo wylaczony."
                } else {
                    Write-Info "Pliki SA:MP juz wylaczone."
                }
            }
            "3" { Write-Info "Kontynuacja bez wylaczania SA:MP." }
            default { throw "Aktualizacja przerwana przez uzytkownika po wykryciu SA:MP." }
        }
    }

    # ── Check for updates using commit SHA ──
    $localSha = Get-LocalCommitSha -gameDir $gtaDir
    $remoteSha = $null
    $needsUpdate = $true

    try {
        $remoteSha = Get-RemoteCommitSha -owner $RepoOwner -repo $RepoName -branch $Branch
        $shortLocal = if ($localSha) { $localSha.Substring(0, 7) } else { "brak" }
        $shortRemote = $remoteSha.Substring(0, 7)
        Write-Info "Zainstalowany commit: $shortLocal"
        Write-Info "Najnowszy commit:    $shortRemote"

        if ($localSha -eq $remoteSha) {
            $needsUpdate = $false
            Write-Ok "Masz juz najnowsza wersje!"
        } else {
            Write-Info "Wykryto nowa wersje - aktualizacja..."
        }
    } catch {
        Write-Info "Nie udalo sie sprawdzic wersji przez GitHub API. Przechodze do aktualizacji."
        $remoteSha = "unknown"
    }

    # ── Even if commit matches, check file integrity ──
    if (-not $needsUpdate) {
        $missingFiles = Test-CoreFilesPresent -gameDir $gtaDir
        if ($missingFiles.Count -gt 0) {
            Write-Err "Brakuje plikow mimo aktualnej wersji: $($missingFiles -join ', ')"
            Write-Info "Wymuszam ponowna instalacje..."
            $needsUpdate = $true
        } else {
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
                }
            }

            Write-Ok "Brak aktualizacji."
            exit 0
        }
    }

    # ── Download and install ──
    if ($CloseRunningProcesses) {
        Stop-IfRunning -processNames @("server", "gta_sa")
    }

    New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

    $zipUrl = "https://codeload.github.com/$RepoOwner/$RepoName/zip/refs/heads/$Branch"
    Write-Info "Pobieram paczke: $zipUrl"
    Invoke-WebRequest -Uri $zipUrl -OutFile $zipPath -UseBasicParsing

    Write-Info "Rozpakowuje..."
    Expand-Archive -Path $zipPath -DestinationPath $tempRoot -Force

    $repoRoot = Get-ChildItem -Path $tempRoot -Directory | Where-Object { $_.Name -like "$RepoName-*" } | Select-Object -First 1
    if (-not $repoRoot) { throw "Nie znaleziono katalogu repo po rozpakowaniu." }

    # ── Backup ──
    $backupDir = Join-Path $gtaDir ("CoopAndreas_backup_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
    New-Item -ItemType Directory -Path $backupDir -Force | Out-Null
    $backupFiles = @("CoopAndreasSA.dll", "server.exe", "proxy.dll", "VERSION.txt", "EAX.dll")
    foreach ($f in $backupFiles) {
        $existing = Join-Path $gtaDir $f
        if (Test-Path $existing) {
            Copy-Item -LiteralPath $existing -Destination (Join-Path $backupDir $f) -Force
        }
    }
    Write-Ok "Backup zapisany do: $(Split-Path -Leaf $backupDir)"

    # ── Full mod install ──
    Install-FullMod -repoRoot $repoRoot.FullName -gameDir $gtaDir

    # ── External dependencies ──
    Ensure-AsiLoader -gameDir $gtaDir
    if (-not $NoResolutionFix) {
        Ensure-WidescreenFix -gameDir $gtaDir
    }

    # ── MSVC Runtime ──
    if (-not (Test-VCRedistInstalled)) {
        Write-Err "Brak Visual C++ Redistributable x86 — CoopAndreasSA.dll nie zaladuje sie bez tego!"
        Install-VCRedist
    } else {
        Write-Ok "Visual C++ Redistributable x86 wykryty."
    }

    # ── DirectX 9 Runtime ──
    if (-not (Test-DirectX9Installed)) {
        Write-Err "Brak DirectX 9 Runtime (d3dx9_43.dll) — CoopAndreasSA.dll nie zaladuje sie bez tego!"
        Install-DirectX9
    } else {
        Write-Ok "DirectX 9 Runtime (d3dx9_43.dll) wykryty."
    }

    # ── Unblock downloaded files (remove Windows 'downloaded from internet' flag) ──
    Unblock-ModFiles -gameDir $gtaDir

    # ── Write VERSION.txt with commit SHA ──
    $latestPkg = Get-ChildItem -Path (Join-Path $repoRoot.FullName "release") -Directory |
        Where-Object { $_.Name -like "old-*" } |
        Sort-Object -Descending -Property @{
            Expression = {
                $vp = ($_.Name -replace '^old-', '')
                if ($vp -match '^\d+(\.\d+){1,3}$') { return [version]$vp }
                return [version]'0.0.0.0'
            }
        } | Select-Object -First 1

    $channelName = if ($latestPkg) { $latestPkg.Name } else { "unknown" }
    Write-VersionFile -gameDir $gtaDir -commitSha $remoteSha -channelName $channelName

    # ── Launchers ──
    Write-LocalLaunchers -gameDir $gtaDir

    Write-Host ""
    Write-Ok "======================================"
    Write-Ok "  Instalacja CoopAndreas zakonczona!"
    Write-Ok "======================================"
    Write-Host ""

    if ($RunAfterUpdate) {
        $serverExe = Join-Path $gtaDir "server.exe"
        $gtaExe = Join-Path $gtaDir "gta_sa.exe"

        if (Test-Path $serverExe) {
            Write-Info "Uruchamiam serwer..."
            Start-Process -FilePath $serverExe -WorkingDirectory $gtaDir -WindowStyle Normal
            Start-Sleep -Seconds 1
        }

        if (Test-Path $gtaExe) {
            Write-Info "Uruchamiam GTA San Andreas..."
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
