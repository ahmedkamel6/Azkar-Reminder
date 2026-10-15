[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$Executable,
    [string]$PackageFullName,
    [string]$PackageFamilyName
)
$ErrorActionPreference = 'Stop'
# Native startup uses the Windows known-folder API and HKCU. Use a throwaway
# runner profile rather than redirecting or replacing a developer's data.
if ($env:CI -ne 'true') { throw 'Requires a disposable CI profile.' }
$Executable = (Resolve-Path -LiteralPath $Executable).Path
$data = Join-Path $env:LOCALAPPDATA 'Azkar Reminder'
if ($PackageFullName) {
    if (!$PackageFamilyName) { throw 'Package family name is required.' }
    $data = Join-Path $env:LOCALAPPDATA "Packages/$PackageFamilyName/LocalCache/Local/Azkar Reminder"
}
if (Test-Path -LiteralPath $data) {
    if (Get-ChildItem -LiteralPath $data -File -Recurse -Force) { throw 'Existing application data found.' }
}
if (!('AzkarSmokeWindow' -as [type])) { Add-Type @'
using System;
using System.Runtime.InteropServices;
using System.Text;
[ComImport, Guid("2e941141-7f97-4756-ba1d-9decde894a3d"), InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
interface IAzkarActivationManager {
    [PreserveSig] int ActivateApplication([MarshalAs(UnmanagedType.LPWStr)] string id,
        [MarshalAs(UnmanagedType.LPWStr)] string args, uint options, out uint processId);
}
[ComImport, Guid("45BA127D-10A8-46EA-8AB7-56EA9078943C")]
class AzkarActivationManager { }
public static class AzkarSmokeWindow {
    [DllImport("user32.dll", CharSet=CharSet.Unicode)]
    private static extern IntPtr FindWindow(string className, string title);
    public static IntPtr MainWindow() { return FindWindow("DhikrMain", null); }
    [DllImport("user32.dll")]
    public static extern uint GetWindowThreadProcessId(IntPtr window, out uint processId);
    [DllImport("user32.dll")]
    public static extern bool IsWindowVisible(IntPtr window);
    [DllImport("kernel32.dll", CharSet=CharSet.Unicode)]
    private static extern int GetPackageFullName(IntPtr process, ref uint length, StringBuilder name);
    public static string PackageName(IntPtr process) {
        uint length = 0;
        int result = GetPackageFullName(process, ref length, null);
        if (result != 122) throw new InvalidOperationException("Package query failed: " + result);
        var name = new StringBuilder((int)length);
        result = GetPackageFullName(process, ref length, name);
        if (result != 0) throw new InvalidOperationException("Package query failed: " + result);
        return name.ToString();
    }
    public static int Activate(string id) {
        var manager = (IAzkarActivationManager)new AzkarActivationManager();
        try {
            uint processId;
            Marshal.ThrowExceptionForHR(manager.ActivateApplication(id, "--background", 0, out processId));
            return (int)processId;
        } finally { Marshal.ReleaseComObject(manager); }
    }
}
'@
}
New-Item -ItemType Directory -Path $data -Force | Out-Null
@{ firstLaunch = $false; startWithWindows = $false; remindersEnabled = $false; language = 'en' } |
    ConvertTo-Json | Set-Content -LiteralPath (Join-Path $data 'settings.json') -Encoding utf8
$process = $null
$clients = [Collections.Generic.List[Diagnostics.Process]]::new()
function Start-Reminder {
    if ($PackageFullName) {
        $script:process = Get-Process -Id ([AzkarSmokeWindow]::Activate("$PackageFamilyName!AzkarReminder"))
        if ([AzkarSmokeWindow]::PackageName($script:process.Handle) -ne $PackageFullName) { throw 'Unexpected process package identity.' }
    } else {
        $script:process = Start-Process -FilePath $Executable -ArgumentList '--background' -PassThru
    }
    for ($attempt = 0; $attempt -lt 30; $attempt++) {
        if ($script:process.HasExited) { throw "Application exited: $($script:process.ExitCode)" }
        $window = [AzkarSmokeWindow]::MainWindow()
        [uint32]$ownerId = 0
        [void][AzkarSmokeWindow]::GetWindowThreadProcessId($window, [ref]$ownerId)
        if ($window -ne [IntPtr]::Zero -and $ownerId -eq $script:process.Id) { return $window }
        Start-Sleep -Seconds 1
    }
    throw 'Native main window was not created.'
}
function Stop-Reminder {
    $quit = Start-Process -FilePath $Executable -ArgumentList '--quit' -PassThru
    $clients.Add($quit)
    if (!$quit.WaitForExit(15000) -or $quit.ExitCode -ne 0) { throw 'Quit client failed.' }
    if (!$script:process.WaitForExit(15000) -or $script:process.ExitCode -ne 0) { throw 'Native shutdown failed.' }
    $script:process = $null
}
try {
    $window = Start-Reminder
    if (!(Test-Path -LiteralPath (Join-Path $data 'content.json'))) { throw 'Application did not use the expected data directory.' }
    if ([AzkarSmokeWindow]::IsWindowVisible($window)) { throw 'Background startup displayed the main window.' }
    $second = Start-Process -FilePath $Executable -PassThru
    $clients.Add($second)
    if (!$second.WaitForExit(15000) -or $second.ExitCode -ne 0) { throw 'Single-instance launch failed.' }
    Start-Sleep -Seconds 1
    if (![AzkarSmokeWindow]::IsWindowVisible($window)) { throw 'Second launch did not show the existing window.' }
    Stop-Reminder
    Set-Content -LiteralPath (Join-Path $data 'content.json') -Value '[]' -NoNewline
    $null = Start-Reminder
    Stop-Reminder
    if ((Get-Content -Raw -LiteralPath (Join-Path $data 'content.json')) -ne '[]') { throw 'Empty library was replaced.' }
    if (!$PackageFullName) {
        $startup = Start-Process -FilePath $Executable -ArgumentList '--setup-autostart' -PassThru
        $clients.Add($startup)
        if (!$startup.WaitForExit(15000) -or $startup.ExitCode -ne 0) { throw 'Startup registration client failed.' }
        $runKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run'
        if ((Get-ItemPropertyValue -LiteralPath $runKey -Name AzkarReminder) -ne "`"$Executable`" --background") { throw 'Startup command does not reference this executable.' }
        $removeStartup = Start-Process -FilePath $Executable -ArgumentList '--remove-autostart' -PassThru
        $clients.Add($removeStartup)
        if (!$removeStartup.WaitForExit(15000) -or $removeStartup.ExitCode -ne 0) { throw 'Startup removal client failed.' }
        if (Get-ItemProperty -LiteralPath $runKey -Name AzkarReminder -ErrorAction SilentlyContinue) { throw 'Startup registration was not removed.' }
    }
    Write-Host 'PASS: native window, background startup, single instance, IPC show/quit and empty library.'
} catch {
    if ($process) { $process | Select-Object Id, SessionId, Responding, MainWindowTitle | Format-List | Out-Host }
    Get-ChildItem -LiteralPath (Join-Path $data 'logs') -File -ErrorAction SilentlyContinue | ForEach-Object {
        Write-Host "Application log: $($_.Name)"
        Get-Content -LiteralPath $_.FullName -Tail 40 | Out-Host
    }
    throw
} finally {
    foreach ($client in $clients) { if (!$client.HasExited) { Stop-Process -Id $client.Id -Force } }
    if ($process -and !$process.HasExited) { Stop-Process -Id $process.Id -Force }
    # The directory was absent before this script and created above.
    Remove-Item -LiteralPath $data -Recurse -Force
}
