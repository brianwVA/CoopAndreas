@echo off
setlocal
title CoopAndreas - Pelne czyszczenie
set "SCRIPT_DIR=%~dp0"
set "PS_EXE=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"

"%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%CoopAndreasCleaner.ps1"
if errorlevel 1 (
  echo.
  echo [BLAD] Czyszczenie nieudane.
  echo Sprawdz komunikat wyzej.
  pause
  exit /b 1
)

echo.
echo [OK] Czyszczenie zakonczone.
pause
exit /b 0
