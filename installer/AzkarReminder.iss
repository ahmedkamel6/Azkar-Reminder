; AzkarReminder.iss — Inno Setup 6 installer script
;
; HOW TO BUILD:
;   1. Install Inno Setup 6 (https://jrsoftware.org/isinfo.php)
;   2. Build the Release EXE first:
;        cmake --build build --config Release
;   3. Compile this script:
;        iscc installer\AzkarReminder.iss
;   4. Output: release\AzkarReminder-Windows-Setup.exe
;
; RELEASE WORKFLOW:
;   1. Bump #define AppVersion below (e.g. 1.1.0)
;   2. Bump FILEVERSION in DhikrReminder.rc to match
;   3. Bump kVersion in src/core/ApplicationMetadata.h to match
;   4. Run scripts\build_release.bat
;   5. Create GitHub Release tag v1.1.0
;   6. Upload release\AzkarReminder-Windows-Setup.exe as the release asset

#define AppName        "Azkar Reminder"
#define AppVersion     "1.2.5"
#define AppPublisher   "Ahmed Mohamed Kamel"
#define AppURL         "https://github.com/ahmedkamel6/Azkar-Reminder"
#define AppId          "{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}"
#define AppUserModelID "AhmedMohamedKamel.AzkarReminder"
#ifndef SourceExe
#define SourceExe      "..\build\bin\Release\AzkarReminder.exe"
#endif

[Setup]
AppId={{#AppId}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
AllowNoIcons=yes
OutputDir=..\release
OutputBaseFilename=AzkarReminder-Windows-Setup
Compression=none
SolidCompression=no
MinVersion=10.0
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
PrivilegesRequiredOverridesAllowed=commandline dialog
UsedUserAreasWarning=no
UninstallDisplayName={#AppName}
UninstallDisplayIcon={app}\AzkarReminder.exe,0
SetupIconFile=..\assets\icons\app.ico
CloseApplications=yes
CloseApplicationsFilter=AzkarReminder.exe,DhikrReminder.exe
RestartApplications=yes
WizardStyle=modern
VersionInfoVersion={#AppVersion}
VersionInfoCompany={#AppPublisher}
VersionInfoDescription=Azkar Reminder - An elegant desktop application to display Dhikr reminders over other windows
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
VersionInfoCopyright=Copyright © 2026 {#AppPublisher}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "autostart"; Description: "Start Azkar Reminder automatically when Windows starts"; GroupDescription: "Additional tasks:"; Flags: checkedonce

[Files]
Source: "{#SourceExe}"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\assets\audio\*"; DestDir: "{app}\audio"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\THIRD_PARTY_NOTICES.md"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\assets\licenses\Cairo-OFL.txt"; DestDir: "{app}\licenses"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";           Filename: "{app}\AzkarReminder.exe"; IconFilename: "{app}\AzkarReminder.exe"; IconIndex: 0; AppUserModelID: "{#AppUserModelID}"
Name: "{group}\Uninstall {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}";     Filename: "{app}\AzkarReminder.exe"; IconFilename: "{app}\AzkarReminder.exe"; IconIndex: 0; AppUserModelID: "{#AppUserModelID}"; Tasks: desktopicon

[Run]
Filename: "{app}\AzkarReminder.exe"; Description: "{cm:LaunchProgram,{#StringChange(AppName, '&', '&&')}}"; Flags: nowait postinstall skipifsilent unchecked
; Create scheduled task if autostart was selected (bypasses schtasks heuristic)
Filename: "{app}\AzkarReminder.exe"; Parameters: "--setup-autostart"; Flags: runhidden; Tasks: autostart

[UninstallRun]
Filename: "{app}\AzkarReminder.exe"; Parameters: "--quit"; RunOnceId: "AzkarReminderQuit"; Flags: runhidden skipifdoesntexist
; Clean up scheduled task
Filename: "{app}\AzkarReminder.exe"; Parameters: "--remove-autostart"; RunOnceId: "DelSchTask"; Flags: runhidden skipifdoesntexist


