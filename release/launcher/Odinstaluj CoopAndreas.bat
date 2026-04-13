@echo off
setlocal
title CoopAndreas - Odinstalowanie
set "SCRIPT_DIR=%~dp0"
set "PS_EXE=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"

"%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%CoopAndreasCleaner.ps1"
if errorlevel 1 (
  echo.
  echo [BLAD] Odinstalowanie nieudane.
  echo Sprawdz komunikat wyzej.
  pause
  exit /b 1
)

echo.
echo [OK] Odinstalowanie zakonczone.
pause
exit /b 0
