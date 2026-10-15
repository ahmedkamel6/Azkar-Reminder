# 1.2.5 package validation

Validation date: 15 October 2026. This is a development candidate, not a published release.

The package acceptance run is [GitHub Actions 38756957740](https://github.com/RlxChap2/Azkar-Reminder/actions/runs/38756957740), built from `53233680f9af75a8fbf5b6b08f426a209baca1ee`. Subsequent documentation changes do not change the runtime source. The workflow ran in the `RlxChap2` fork because the connected account cannot push to the original repository.

| Build | Environment | Checks |
|---|---|---|
| Windows native x64, Setup and MSIX | Windows Server 2025 x64 runner | Release build; 23 core cases; native window creation; background startup; second-instance show; IPC quit; empty library; startup registration/removal; Setup install/reinstall/uninstall with user data retained; MSIX test signing, checksum, installation, AUMID activation, process package identity and removal |
| Optional Windows Qt frontend | Windows runner, official Qt 6.8.3 MSVC SDK | Both Windows targets compile; core, audio retry and UI CTest groups pass |
| Linux x86_64 AppImage | Ubuntu 22.04 runner and minimal Ubuntu 22.04 container without Qt | Core/audio/UI groups; explicit Xvfb fullscreen test; executable IPC/data recovery; core-only build; packaged startup, IPC and data recovery; checksum after moving the artifact |
| macOS universal preview DMG | macOS 14.8.9 ARM runner, official Qt 6.8.3 SDK | Both slices in executable/frameworks/plugins; core/audio/UI groups; ad-hoc signature verification; relocated checksum; read-only DMG mount; deployed Cocoa app; Launch Services; IPC/data recovery; x86_64 launch and quit under Rosetta |

The first headless Linux CTest invocation intentionally skips X11 when no display is available. The workflow then runs that test explicitly under Xvfb and requires success. The Linux suite has 24 core cases and eight Qt Test entries, including setup and cleanup. Windows and macOS have 23 core cases and seven Qt Test entries because the extra notification case requires Linux D-Bus.

Earlier Ubuntu 22.04.5 VMs also exercised X11 and headless Weston/Wayland in a separate guest without Qt installed. These VM results predate the final IPC changes. The current AppImage is checked independently in the minimal container. The shared core previously passed AddressSanitizer and UndefinedBehaviorSanitizer in Ubuntu; those sanitizer results predate the final scheduler state repair.

## Package tests

`scripts/smoke_linux.sh` isolates XDG data and checks the actual executable or AppImage. It verifies that the instance lock belongs to the launched process or an AppImage child before sending readiness probes. This avoids accidentally making the probe itself the primary instance.

`scripts/smoke_windows_packages.ps1` uses a disposable runner profile. It compares installed executables with the freshly built binary, preserves a data marker across Setup reinstallation/removal, and signs a temporary MSIX copy. The test certificate is trusted only inside the throwaway runner and removed afterwards. The uploaded MSIX remains unsigned and contains no test certificate/private key.

`scripts/smoke_macos.sh` removes Qt SDK plugin-path overrides, checks the copied checksum and bundle signature, then launches the deployed app. On ARM it also executes the x86_64 slice through Rosetta. IPC clients require an acknowledgement from the running instance; a successful socket write alone is insufficient.

The checks exposed and prompted fixes for a scheduler pause-state race, missing HarfBuzz in minimal Linux environments, macOS `lipo` argument ordering, a missing Windows startup key, native shutdown ordering, preservation after a failed settings backup, and IPC startup/readiness handling. The smoke scripts were also corrected for system Bash behavior and AppImage parent/child processes.

## Limits

- The macOS preview is ad-hoc signed. Developer ID signing, notarization and Gatekeeper after an Internet download have not been tested with real credentials. A tag build deliberately fails without those credentials.
- Rosetta execution is not an Intel hardware test. macOS 12 is the deployment target, but the runtime host tested here is macOS 14.
- Windows installation ran on Server 2025. A complete Windows 10/11 desktop session, upgrade from an older release and Microsoft Store certification remain separate checks. The Setup test reinstalls the same candidate version; it does not prove an upgrade from 1.2.4. MSIX upgrade was not exercised.
- No test establishes audible quality on physical speakers, real login startup, multiple physical displays, Retina/DPI transitions, Spaces, GNOME/KDE integration or physical sleep/wake behavior. Store autostart requires a `StartupTask` review rather than relying on virtualized HKCU Run writes.
- The MP3 provenance and religious text references still require review before a public release. See [remaining work](AUDIT_AND_ROADMAP.md).

No release tag, public release or Store submission was created during validation.
