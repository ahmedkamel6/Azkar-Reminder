# Changelog

## 1.2.5 (development candidate)

- Fixed disabled content selection, loss of disabled entries, non-BMP Unicode, settings validation and failed saves reported as success.
- Preserved intentionally empty libraries and user files. Default dhikr content now has one source in `assets/content.md`.
- Fixed overlapping daily schedules, restored saved pause state and prevented stale preferences from replacing the live pause state.
- Fixed miniaudio retry deadlock and made Stop control the actual playback. Null backends are reported as unavailable.
- Added Qt weekly schedules, display controls and library import/export/disable controls. Fixed persisted language and quiet hours, long text scrolling, missing-tray exit and second-instance IPC.
- Replaced fatal Xlib error paths with checked XCB replies. AppImage startup registration uses the permanent APPIMAGE path.
- Added pinned Qt deployment packaging for Linux and a macOS universal/signing/notarization pipeline that fails on deployment errors.
- Added regression, audio, UI and executable smoke tests. Updated build, privacy and remaining-work documentation without resource-use or antivirus guarantees.

Package validation ran in Ubuntu VMs and GitHub-hosted Linux, Windows and macOS environments. Windows Setup installation/reinstallation/removal and a test-signed MSIX activation passed. The macOS DMG mounted and its deployed Cocoa app passed startup, IPC and library recovery checks. See `docs/BUILD_VALIDATION.md` for evidence and limits. This entry does not record a published release.

- Fixed a scheduler state race exposed by macOS: a worker snapshot could overwrite a completed pause request.
- Explicitly bundled HarfBuzz after a minimal Ubuntu container exposed the missing runtime library.
- Fixed macOS `lipo` argument ordering and exercised the package smoke script with the system Bash.
- Created the Windows startup registry key when needed and shut down native subsystems before static destruction.
- Preserved malformed settings when backup fails, and acknowledged IPC requests only after UI startup is ready.
