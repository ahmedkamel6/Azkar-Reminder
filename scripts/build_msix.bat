@echo off
setlocal
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0..\installer\msix\build_msix.ps1" %*
exit /b %errorlevel%
