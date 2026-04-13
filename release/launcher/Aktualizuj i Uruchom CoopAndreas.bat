@echo off
setlocal
title CoopAndreas - Aktualizuj i uruchom
set "SCRIPT_DIR=%~dp0"
set "PS_EXE=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"

"%PS_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%CoopAndreasUpdater.ps1" -RunAfterUpdate
if errorlevel 1 (
  echo.
  echo [BLAD] Aktualizacja lub uruchomienie nieudane.
  echo Sprawdz komunikat wyzej.
  pause
  exit /b 1
)

echo.
echo [OK] Aktualizacja i uruchomienie wykonane.
exit /b 0
