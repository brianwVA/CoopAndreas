@echo off
setlocal
title CoopAndreas - Aktualizacja
set "SCRIPT_DIR=%~dp0"
set "PS_EXE=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
set "PS1_FILE=%SCRIPT_DIR%CoopAndreasUpdater.ps1"

if not exist "%PS1_FILE%" (
  echo [BLAD] Brak pliku CoopAndreasUpdater.ps1
  echo Umiesc go w tym samym folderze co ten plik .bat
  pause
  exit /b 1
)

echo [INFO] Uruchamiam CoopAndreas Updater...
"%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -Command "& { try { & '%PS1_FILE%' } catch { Write-Host ('[BLAD] ' + $_.Exception.Message) -ForegroundColor Red; exit 1 }; exit $LASTEXITCODE }"
set "EC=%errorlevel%"
if %EC% neq 0 (
  echo.
  echo [BLAD] Aktualizacja nieudana (kod: %EC%).
  echo Sprawdz komunikat wyzej.
  pause
  exit /b %EC%
)

echo.
echo [OK] Aktualizacja zakonczona.
pause
exit /b 0
