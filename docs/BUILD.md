# Building Azkar Reminder 1.2.5

CMake 3.20 or newer and a C++20 compiler are required. Default content comes from `assets/content.md`; CMake generates the embedded header from that file. Qt embeds Cairo and the logo through `assets/qt.qrc`.

## Linux

Ubuntu 22.04 provides the tested Qt 6.2.4 baseline:

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build g++ qt6-base-dev qt6-base-dev-tools qt6-wayland \
  libxcb1-dev libgl1-mesa-dev libasound2-dev libpulse-dev file curl patchelf dbus-x11
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_QT_APP=ON -DBUILD_TESTING=ON
cmake --build build --parallel 4
dbus-run-session -- ctest --test-dir build --output-on-failure
dbus-run-session -- bash scripts/smoke_linux.sh build/azkar_reminder
bash scripts/package_linux.sh build release
```

The packaging script verifies pinned SHA-256 digests for linuxdeploy and its Qt plugin. It deploys Qt libraries, X11/Wayland platform plugins, offscreen support for smoke tests, Cairo, audio, and license notices. The output is `release/AzkarReminder-Linux-x86_64.AppImage` with a SHA-256 file. This script targets Debian/Ubuntu x86_64 build hosts; it uses `dpkg-query` for bundled-library notices.

Test the artifact on a second system without Qt, not only on the build machine:

```bash
bash scripts/smoke_linux.sh release/AzkarReminder-Linux-x86_64.AppImage --appimage-extract-and-run
```

The smoke script isolates XDG data and checks startup, second-instance IPC, shutdown, an intentionally empty library and a malformed library. A minimal container still needs `libgl1 libegl1 libopengl0 libfontconfig1 libxkbcommon0 libdbus-1-3`; these are display/runtime dependencies, not Qt. For X11 use `xvfb-run -a env QT_QPA_PLATFORM=xcb bash scripts/smoke_linux.sh ...`. A headless compositor smoke test does not establish GNOME/KDE tray behavior or actual audio output.

Core-only configuration:

```bash
cmake -S . -B build-core -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_QT_APP=OFF
cmake --build build-core --parallel 4
ctest --test-dir build-core --output-on-failure
```

## Windows

Install Visual Studio 2022 with Desktop development with C++, MSVC v143 and a Windows SDK, plus CMake and Inno Setup 6.

```powershell
cmake -S . -B build -DBUILD_QT_APP=OFF -DBUILD_TESTING=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
& 'C:\Program Files (x86)\Inno Setup 6\ISCC.exe' installer\AzkarReminder.iss
```

The native executable is `build/bin/Release/AzkarReminder.exe`; its audio directory is copied beside it. The installer is `release/AzkarReminder-Windows-Setup.exe`. `scripts/build_release.bat` automates the native build and installer. `verify_release.bat` checks files, not actual installation behavior.

Optional Qt Windows builds require a matching Qt SDK and `BUILD_QT_APP=ON`. Both frontends build and the core/audio/UI tests pass with Qt 6.8.3 on a Windows runner. MSIX packaging is available in `installer/msix`. Run `./installer/msix/build_msix.ps1` to rebuild and package with the registered Store identity. It reads the CMake version, rejects a mismatched binary and checks MakeAppx failures. It produces an unsigned MSIX by default. See [MSIX instructions](../installer/msix/README.md) for explicit certificate-based signing.

`smoke_windows.ps1` and `smoke_windows_packages.ps1` require a disposable CI profile. They exercise native windows and IPC, Setup installation/reinstallation/removal, and a test-signed MSIX activated through its AUMID. The MSIX check asserts the process package identity and uses its private AppData fixtures. Test certificate trust is added and removed only in the throwaway runner; the distributable remains unsigned. Do not set `CI=true` to run these scripts against a working user profile. See [recorded validation](BUILD_VALIDATION.md).

## macOS

Use Xcode's command line tools and the official Qt 6.8.3 `clang_64` SDK containing both architectures. Homebrew Qt on a single-architecture host is insufficient for a universal bundle. The workflow uses `jurplel/install-qt-action@v4` with version `6.8.3`.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_QT_APP=ON -DBUILD_TESTING=ON \
  '-DCMAKE_OSX_ARCHITECTURES=x86_64;arm64' -DCMAKE_OSX_DEPLOYMENT_TARGET=12.0 \
  -DCMAKE_PREFIX_PATH=/path/to/Qt/6.8.3/macos
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
bash scripts/package_macos.sh build release
```

Put Qt's `bin` directory on `PATH` so `macdeployqt` is available. Packaging deploys Qt, generates an `.icns`, verifies both architectures in the executable/frameworks/plugins, and creates a DMG containing the app and an Applications shortcut. Deployment errors stop the build. Without credentials the preview is named `Unsigned`, uses explicit ad-hoc signing without hardened runtime, and is verified with codesign. This is not Developer ID signing or notarization.

For a signed bundle, configure `MACOS_SIGNING_IDENTITY` and `MACOS_NOTARY_PROFILE`. The workflow imports credentials from these repository secrets:

- `MACOS_SIGNING_IDENTITY`, `MACOS_CERTIFICATE_BASE64`, `MACOS_CERTIFICATE_PASSWORD`, `MACOS_KEYCHAIN_PASSWORD`.
- `MACOS_APPLE_ID`, `MACOS_APPLE_TEAM_ID`, `MACOS_APP_PASSWORD`.

The certificate must contain a Developer ID Application identity. The workflow stores a temporary notary profile, signs, requires notarization status `Accepted`, staples tickets, and removes the temporary keychain. Release tags fail when signing credentials are missing. Development branch artifacts use ad-hoc signing. Developer ID signing and notarization have not been executed with real credentials.

On a disposable macOS CI profile, `bash scripts/smoke_macos.sh release/*.dmg` mounts the DMG read-only, copies and verifies the app, removes SDK plugin-path overrides, and exercises Cocoa startup, Launch Services, IPC and library recovery. On an ARM runner it also runs the deployed x86_64 slice through Rosetta. This does not replace an Intel hardware or Gatekeeper test.

Before publishing, test Intel and Apple Silicon launch, Gatekeeper, startup after logout/login, audio, fullscreen Spaces, Retina and multiple displays on macOS. The current LaunchAgent startup adapter remains a compatibility implementation; a tested `SMAppService` migration is deferred.

## CI and versioning

The workflow runs CTest on each host, builds the optional Windows Qt frontend and exercises distribution packages. Linux packaging is tested in a minimal container without Qt. Recorded runs and their limits are in [BUILD_VALIDATION.md](BUILD_VALIDATION.md). Tag builds create a draft prerelease only after all required jobs pass.

Keep `project(... VERSION ...)`, Windows `DhikrReminder.rc` values and the Inno `AppVersion` synchronized. C++ and Qt display the CMake-provided `AZKAR_APP_VERSION`.

Primary references: [Qt Linux deployment](https://doc.qt.io/qt-6/linux-deployment.html), [linuxdeploy Qt plugin options](https://github.com/linuxdeploy/linuxdeploy-plugin-qt), [Qt 6.8 supported platforms](https://doc.qt.io/qt-6.8/supported-platforms.html), [install-qt-action](https://github.com/jurplel/install-qt-action).
