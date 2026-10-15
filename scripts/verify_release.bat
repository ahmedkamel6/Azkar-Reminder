@echo off
setlocal

cd /d "%~dp0.."

echo ===================================================
echo   Azkar Reminder - Release Readiness Verification
echo ===================================================
echo.

set FAILED=0

:: 1. Check executable
if not exist "build\bin\Release\AzkarReminder.exe" (
    echo [FAIL] build\bin\Release\AzkarReminder.exe is missing.
    set FAILED=1
) else (
    echo [PASS] Application executable AzkarReminder.exe found.
)

:: 2. Check external audio asset in output
if not exist "build\bin\Release\audio\diode_matrix.mp3" (
    echo [FAIL] build\bin\Release\audio\diode_matrix.mp3 is missing.
    set FAILED=1
) else (
    echo [PASS] External notification audio asset found.
)

:: 3. Check installer
if not exist "release\AzkarReminder-Windows-Setup.exe" (
    echo [FAIL] release\AzkarReminder-Windows-Setup.exe is missing. Run build_release.bat to generate it.
    set FAILED=1
) else (
    echo [PASS] Official Windows Installer AzkarReminder-Windows-Setup.exe found.
)

echo.
if %FAILED% equ 1 (
    echo ===================================================
    echo   Release verification FAILED!
    echo ===================================================
    exit /b 1
)

echo ===================================================
echo   Release verification PASSED! Ready for distribution.
echo ===================================================
exit /b 0
