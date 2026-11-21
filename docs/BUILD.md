# Building Azkar Reminder from Source

This guide provides step-by-step instructions for compiling Azkar Reminder and generating the Windows Installer on a clean Windows x64 machine.

## Prerequisites
To compile the project and installer, you will need:
1. **Windows 10 or 11 (x64)**
2. **Visual Studio 2022** (or Build Tools) with:
   - Desktop development with C++
   - Windows 10/11 SDK
   - MSVC v143 - VS 2022 C++ x64/x86 build tools
3. **CMake** (version 3.20 or newer)
4. **Inno Setup 6** (available from [https://jrsoftware.org/isinfo.php](https://jrsoftware.org/isinfo.php))

## Automated Build & Packaging
To build both the Release executable and the Windows Installer in one command, run:

```bat
scripts\build_release.bat
```

This will:
1. Configure and build the Release executable (`build\bin\Release\AzkarReminder.exe`) with all static assets (logo, application icons, default library) embedded directly into the binary resources.
2. Copy the external audio directory to `build\bin\Release\audio\`.
3. Compile the Inno Setup script into `release\AzkarReminder-Windows-Setup.exe`.

## Manual Build Steps

### 1. Configure the Build
```bat
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
```

### 2. Build the Executable
```bat
cmake --build build --config Release
```

Output files will be located in `build\bin\Release`:
- `AzkarReminder.exe` (self-contained with embedded visual & content resources)
- `audio\` (external notification audio)

### 3. Build the Installer
Compile the installer using Inno Setup:

```bat
iscc installer\AzkarReminder.iss
```
(Or use the full path to `ISCC.exe`, e.g., `"%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"` or `"C:\Program Files (x86)\Inno Setup 6\ISCC.exe"`).

The resulting installer `AzkarReminder-Windows-Setup.exe` will be located in `release\`.

## 4. Verification
Run the verification script to confirm build readiness:
```bat
scripts\verify_release.bat
```
