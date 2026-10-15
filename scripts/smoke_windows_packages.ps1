[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SetupPath,
    [Parameter(Mandatory)][string]$PackagePath,
    [Parameter(Mandatory)][string]$ReferenceExecutable
)
$ErrorActionPreference = 'Stop'
if ($env:CI -ne 'true' -or !$env:RUNNER_TEMP) { throw 'Requires a disposable GitHub Actions runner.' }
$root = Split-Path $PSScriptRoot -Parent
$taskDirectory = Join-Path $env:RUNNER_TEMP ([Guid]::NewGuid().ToString('N'))
$installDirectory = Join-Path $taskDirectory 'installed app'
$data = Join-Path $env:LOCALAPPDATA 'Azkar Reminder'
if (Test-Path -LiteralPath $data) { throw 'Existing application data found.' }
New-Item -ItemType Directory -Path $taskDirectory | Out-Null
$SetupPath = (Resolve-Path -LiteralPath $SetupPath).Path
$PackagePath = (Resolve-Path -LiteralPath $PackagePath).Path
$referenceHash = (Get-FileHash -LiteralPath $ReferenceExecutable).Hash
$certificate = $null
$installedPackage = $null
function Invoke-Installer([string]$Path, [string[]]$Arguments) {
    $installer = Start-Process -FilePath $Path -ArgumentList $Arguments -PassThru
    if (!$installer.WaitForExit(120000)) {
        Stop-Process -Id $installer.Id -Force
        throw 'Installer timed out.'
    }
    return $installer.ExitCode
}
try {
    $arguments = @('/CURRENTUSER', '/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART', '/TASKS=""', "/DIR=`"$installDirectory`"")
    $setupExit = Invoke-Installer $SetupPath $arguments
    if ($setupExit -ne 0) { throw "Setup failed: $setupExit" }
    $installedExecutable = Join-Path $installDirectory 'AzkarReminder.exe'
    if ((Get-FileHash -LiteralPath $installedExecutable).Hash -ne $referenceHash) { throw 'Installer contains a stale binary.' }
    & "$PSScriptRoot/smoke_windows.ps1" -Executable $installedExecutable
    New-Item -ItemType Directory -Path $data | Out-Null
    $marker = Join-Path $data 'upgrade-preservation.txt'
    Set-Content -LiteralPath $marker -Value 'preserve user data'
    $upgradeExit = Invoke-Installer $SetupPath $arguments
    if ($upgradeExit -ne 0 -or !(Test-Path -LiteralPath $marker)) { throw 'Reinstall did not preserve user data.' }
    $uninstallExit = Invoke-Installer (Join-Path $installDirectory 'unins000.exe') @('/VERYSILENT', '/SUPPRESSMSGBOXES', '/NORESTART')
    if ($uninstallExit -ne 0 -or (Test-Path -LiteralPath $installedExecutable)) { throw 'Uninstall failed.' }
    if (!(Test-Path -LiteralPath $marker)) { throw 'Uninstall removed user data.' }
    Remove-Item -LiteralPath $data -Recurse -Force
    Write-Host 'PASS: Setup install, installed executable, reinstall and uninstall with user data retained.'

    # Signing and trust changes exist only in this disposable runner. Keep the
    # distributable unsigned package unchanged for Store submission.
    $signedPackage = Join-Path $taskDirectory 'runtime-test.msix'
    Copy-Item -LiteralPath $PackagePath -Destination $signedPackage
    $certificate = New-SelfSignedCertificate -Type Custom -Subject 'CN=BD257909-6037-4C23-A6F3-A71B4BD2AF69' `
        -KeyUsage DigitalSignature -KeyAlgorithm RSA -KeyLength 2048 -HashAlgorithm SHA256 `
        -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}') -CertStoreLocation 'Cert:\CurrentUser\My'
    $certificatePath = Join-Path $taskDirectory 'runtime-test.cer'
    Export-Certificate -Cert $certificate -FilePath $certificatePath | Out-Null
    Import-Certificate -FilePath $certificatePath -CertStoreLocation 'Cert:\LocalMachine\TrustedPeople' | Out-Null
    & "$root/installer/msix/sign_msix.ps1" -PackagePath $signedPackage -CertificateThumbprint $certificate.Thumbprint
    if (!(Get-Content -Raw -LiteralPath "$signedPackage.sha256").StartsWith((Get-FileHash -LiteralPath $signedPackage).Hash.ToLowerInvariant())) { throw 'Signed package checksum is stale.' }
    Add-AppxPackage -Path $signedPackage
    $installedPackage = Get-AppxPackage -Name '15145Ahmedkamel.AzkarReminder'
    if (!$installedPackage) { throw 'MSIX was not registered.' }
    $packageExecutable = Join-Path $installedPackage.InstallLocation 'AzkarReminder.exe'
    if ((Get-FileHash -LiteralPath $packageExecutable).Hash -ne $referenceHash) { throw 'MSIX contains a stale binary.' }
    & "$PSScriptRoot/smoke_windows.ps1" -Executable $packageExecutable -PackageFullName $installedPackage.PackageFullName -PackageFamilyName $installedPackage.PackageFamilyName
    Write-Host 'PASS: MSIX test signing, checksum, installation and installed executable.'
} finally {
    try {
        if ($installedPackage) { Remove-AppxPackage -Package $installedPackage.PackageFullName }
    } finally {
        if ($certificate) {
            Remove-Item -LiteralPath "Cert:\LocalMachine\TrustedPeople\$($certificate.Thumbprint)" -ErrorAction Continue
            Remove-Item -LiteralPath "Cert:\CurrentUser\My\$($certificate.Thumbprint)" -ErrorAction Continue
        }
        $resolved = [IO.Path]::GetFullPath($taskDirectory)
        if (!$resolved.StartsWith([IO.Path]::GetFullPath($env:RUNNER_TEMP) + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Unexpected test directory.' }
        Remove-Item -LiteralPath $resolved -Recurse -Force
    }
}
