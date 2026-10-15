@echo off
setlocal

cd /d "%~dp0.."

echo ===================================================
echo   Azkar Reminder - Release Build and Packaging
echo ===================================================
echo.

echo [1/4] Checking prerequisites...
where cmake >nul 2>nul
if %ERRORLEVEL% neq 0 (
    echo Error: CMake not found in PATH.
    exit /b 1
)

echo [2/4] Configuring CMake (Release)...
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
if %ERRORLEVEL% neq 0 (
    echo Error: CMake configuration failed.
    exit /b 1
)

echo.
echo [3/4] Building AzkarReminder (Release)...
cmake --build build --config Release
if %ERRORLEVEL% neq 0 (
    echo Error: CMake build failed.
    exit /b 1
)

echo.
echo [4/4] Verifying build output...
if not exist "build\bin\Release\AzkarReminder.exe" (
    echo Error: build\bin\Release\AzkarReminder.exe not found.
    exit /b 1
)
if not exist "build\bin\Release\audio\diode_matrix.mp3" (
    echo Error: build\bin\Release\audio\diode_matrix.mp3 not found.
    exit /b 1
)

echo Binary and assets built successfully!
echo.

:: ── Locate Inno Setup Compiler ──────────────────────────────────────────────
echo Looking for Inno Setup 6 compiler...
set "ISCC_EXE="

where iscc >nul 2>nul
if %ERRORLEVEL% equ 0 set "ISCC_EXE=iscc"

if not defined ISCC_EXE if exist "%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe" set "ISCC_EXE=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not defined ISCC_EXE if exist "C:\Program Files (x86)\Inno Setup 6\ISCC.exe" set "ISCC_EXE=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if not defined ISCC_EXE if exist "C:\Program Files\Inno Setup 6\ISCC.exe" set "ISCC_EXE=C:\Program Files\Inno Setup 6\ISCC.exe"

if not defined ISCC_EXE (
    echo Warning: Inno Setup 6 compiler ISCC.exe was not found.
    echo Release binary is ready, but the installer could not be generated.
    echo Please install Inno Setup 6 from https://jrsoftware.org/isinfo.php
    exit /b 0
)

echo Found Inno Setup at: %ISCC_EXE%
echo Compiling Windows Installer...

if not exist "release" mkdir release

"%ISCC_EXE%" "installer\AzkarReminder.iss"
if %ERRORLEVEL% neq 0 (
    echo Error: Inno Setup compilation failed.
    exit /b 1
)

if not exist "release\AzkarReminder-Windows-Setup.exe" (
    echo Error: release\AzkarReminder-Windows-Setup.exe not found after compilation.
    exit /b 1
)

echo.
echo ===================================================
echo   BUILD AND INSTALLER COMPLETED SUCCESSFULLY!
echo ===================================================
echo   Executable: %CD%\build\bin\Release\AzkarReminder.exe
echo   Installer:  %CD%\release\AzkarReminder-Windows-Setup.exe
echo ===================================================

exit /b 0
