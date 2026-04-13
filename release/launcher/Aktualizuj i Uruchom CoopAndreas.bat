@echo off
setlocal
set "SCRIPT_DIR=%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%CoopAndreasUpdater.ps1" -RunAfterUpdate
if errorlevel 1 (
  echo.
  echo [BLAD] Aktualizacja lub uruchomienie nieudane.
  pause
  exit /b 1
)

exit /b 0
