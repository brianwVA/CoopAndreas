@echo off
setlocal
set "SCRIPT_DIR=%~dp0"

powershell -NoProfile -ExecutionPolicy Bypass -File "%SCRIPT_DIR%CoopAndreasUpdater.ps1"
if errorlevel 1 (
  echo.
  echo [BLAD] Aktualizacja nieudana.
  pause
  exit /b 1
)

echo.
echo [OK] Aktualizacja zakonczona.
pause
exit /b 0
