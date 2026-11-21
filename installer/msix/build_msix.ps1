# build_msix.ps1 - Builds the official MSIX package for Microsoft Store & Windows 10/11
[CmdletBinding()]
param (
    [string]$Version = "1.2.5",
    [string]$Publisher = "CN=BD257909-6037-4C23-A6F3-A71B4BD2AF69",
    [string]$PublisherDisplayName = "Ahmedkamel",
    [string]$PackageName = "15145Ahmedkamel.AzkarReminder",
    [switch]$SignForLocalTest = $true
)

$ErrorActionPreference = "Stop"

$root = Resolve-Path (Join-Path $PSScriptRoot "..\..")
$msixDir = $PSScriptRoot
$stagingDir = Join-Path $root "build\msix_staging"
$outputDir = Join-Path $root "release"
$outputMsix = Join-Path $outputDir "AzkarReminder-$Version.msix"
$cerPath = Join-Path $outputDir "AzkarReminder-DeveloperCert.cer"

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "   Azkar Reminder - Microsoft Store (MSIX) Package Builder" -ForegroundColor Cyan
Write-Host "   Version: $Version" -ForegroundColor Cyan
Write-Host "==========================================================" -ForegroundColor Cyan

# 1. Locate Windows SDK Tools
$makeAppx = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\makeappx.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
$signtool = (Get-ChildItem "C:\Program Files (x86)\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName

if (!$makeAppx) {
    throw "makeappx.exe not found in Windows SDK"
}
Write-Host "[1/6] Found makeappx.exe: $makeAppx" -ForegroundColor Green

# 2. Check compiled binary
$exeSource = Join-Path $root "build\bin\Release\AzkarReminder.exe"
if (!(Test-Path $exeSource)) {
    Write-Host "Release binary not found. Building with CMake..." -ForegroundColor Yellow
    & cmake --build (Join-Path $root "build") --config Release
    if (!(Test-Path $exeSource)) {
        throw "Failed to find or build $exeSource"
    }
}
Write-Host "[2/6] Executable verified: $exeSource" -ForegroundColor Green

# 3. Ensure assets exist
$assetsDir = Join-Path $msixDir "Assets"
if (!(Test-Path (Join-Path $assetsDir "StoreLogo.png"))) {
    Write-Host "Generating visual assets..." -ForegroundColor Yellow
    & powershell -ExecutionPolicy Bypass -File (Join-Path $msixDir "create_assets.ps1")
}
Write-Host "[3/6] Visual assets verified in $assetsDir" -ForegroundColor Green

# 4. Prepare Staging Directory
Write-Host "[4/6] Preparing MSIX staging directory..." -ForegroundColor Yellow
if (Test-Path $stagingDir) {
    Remove-Item -Recurse -Force $stagingDir
}
New-Item -ItemType Directory -Path $stagingDir -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stagingDir "audio") -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $stagingDir "Assets") -Force | Out-Null
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

Copy-Item $exeSource -Destination (Join-Path $stagingDir "AzkarReminder.exe")
Copy-Item (Join-Path $root "assets\audio\*") -Destination (Join-Path $stagingDir "audio") -Recurse -Force
Copy-Item (Join-Path $assetsDir "*") -Destination (Join-Path $stagingDir "Assets") -Recurse -Force
if (Test-Path (Join-Path $root "assets\content.md")) {
    Copy-Item (Join-Path $root "assets\content.md") -Destination (Join-Path $stagingDir "content.md")
}

# Update version & identity in AppxManifest.xml
[xml]$manifest = Get-Content (Join-Path $msixDir "AppxManifest.xml")
$manifest.Package.Identity.Version = "$Version.0"
$manifest.Package.Identity.Publisher = $Publisher
$manifest.Package.Identity.Name = $PackageName
$manifest.Package.Properties.PublisherDisplayName = $PublisherDisplayName
$manifest.Save((Join-Path $stagingDir "AppxManifest.xml"))

# 5. Pack MSIX with makeappx.exe
Write-Host "[5/6] Packing MSIX package..." -ForegroundColor Yellow
if (Test-Path $outputMsix) {
    Remove-Item -Force $outputMsix
}
& $makeAppx pack /d $stagingDir /p $outputMsix /o
if (!(Test-Path $outputMsix)) {
    throw "makeappx pack failed!"
}
Write-Host "MSIX package created: $outputMsix" -ForegroundColor Green

# 6. Sign for Local Testing & Sideloading
if ($SignForLocalTest -and $signtool) {
    Write-Host "[6/6] Generating developer test certificate and signing MSIX for local installation..." -ForegroundColor Yellow
    $certSubject = $Publisher
    $certStore = "Cert:\CurrentUser\My"

    $cert = Get-ChildItem $certStore | Where-Object { $_.Subject -eq $certSubject } | Select-Object -First 1
    if (!$cert) {
        Write-Host "Creating self-signed code signing certificate..." -ForegroundColor Cyan
        $cert = New-SelfSignedCertificate `
            -Type Custom `
            -Subject $certSubject `
            -DnsName "AzkarReminder" `
            -KeyUsage DigitalSignature `
            -FriendlyName "Azkar Reminder Local Developer Certificate" `
            -CertStoreLocation $certStore `
            -TextExtension @("2.5.29.37={text}1.3.6.1.5.5.7.3.3")
    }

    Export-Certificate -Cert $cert -FilePath $cerPath -Force | Out-Null
    Write-Host "Public test certificate exported to: $cerPath" -ForegroundColor Green

    & $signtool sign /fd SHA256 /a /sha1 $cert.Thumbprint $outputMsix
    Write-Host "MSIX package signed successfully with local developer certificate!" -ForegroundColor Green
}

Remove-Item -Recurse -Force $stagingDir

$fileSizeMb = [Math]::Round((Get-Item $outputMsix).Length / 1MB, 2)

Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host "MSIX PACKAGE READY FOR MICROSOFT STORE AND WINDOWS 10/11!" -ForegroundColor Green
Write-Host "File: $outputMsix" -ForegroundColor White
Write-Host "Size: $fileSizeMb MB" -ForegroundColor White
Write-Host "==========================================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Microsoft Store submission instructions:" -ForegroundColor Yellow
Write-Host "1. Login to Microsoft Partner Center: https://partner.microsoft.com/"
Write-Host "2. Create a new app reservation for Azkar Reminder"
Write-Host "3. Upload: $outputMsix"
Write-Host "   (Microsoft Store will inspect, validate, and sign it automatically)"
Write-Host ""
Write-Host "Direct local installation:" -ForegroundColor Yellow
Write-Host "1. Open test certificate: $cerPath"
Write-Host "2. Click Install Certificate -> Local Machine -> Trusted People"
Write-Host "3. Double-click $outputMsix to install with 1 click!"
