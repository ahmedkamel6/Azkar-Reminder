[CmdletBinding()]
param(
    [string]$Version,
    [string]$BuildDirectory,
    [string]$OutputDirectory,
    [string]$Publisher,
    [string]$PublisherDisplayName,
    [string]$PackageName,
    [switch]$SignForLocalTest,
    [string]$CertificateThumbprint
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..\..')).Path
if (!$Version) {
    $projectText = Get-Content -LiteralPath (Join-Path $projectRoot 'CMakeLists.txt') -Raw
    if ($projectText -notmatch 'project\(DhikrReminder VERSION ([0-9]+\.[0-9]+\.[0-9]+)') { throw 'CMake project version not found.' }
    $Version = $Matches[1]
}
if ($Version -notmatch '^\d+\.\d+\.\d+$') { throw 'Version must have three numeric components.' }
if (!$BuildDirectory) { $BuildDirectory = Join-Path $projectRoot 'build' }
if (!$OutputDirectory) { $OutputDirectory = Join-Path $projectRoot 'release' }
$BuildDirectory = [IO.Path]::GetFullPath($BuildDirectory)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)

function Invoke-Checked {
    param([string]$Tool, [string[]]$Arguments)
    & $Tool @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Tool failed with exit code $LASTEXITCODE" }
}

$makeAppx = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\makeappx.exe" -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if (!$makeAppx) { throw 'Install the Windows SDK: makeappx.exe was not found.' }
$cmakeCommand = Get-Command cmake -ErrorAction SilentlyContinue
$cmakeTool = if ($cmakeCommand) { $cmakeCommand.Source } else { $null }
if (!$cmakeTool) {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $cmakeTool = & $vswhere -latest -products '*' -find 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe' | Select-Object -First 1
    }
}
if (!$cmakeTool) { throw 'Install CMake or Visual Studio CMake tools.' }
if (!(Test-Path -LiteralPath (Join-Path $BuildDirectory 'CMakeCache.txt'))) {
    Invoke-Checked $cmakeTool @('-S', $projectRoot, '-B', $BuildDirectory, '-A', 'x64', '-DBUILD_QT_APP=OFF')
}
Invoke-Checked $cmakeTool @('--build', $BuildDirectory, '--config', 'Release', '--parallel', '4')
$executable = Join-Path $BuildDirectory 'bin\Release\AzkarReminder.exe'
if (!(Test-Path -LiteralPath $executable)) { throw "Release executable not found: $executable" }
$binaryVersion = [Diagnostics.FileVersionInfo]::GetVersionInfo($executable).ProductVersion
if ([version]$binaryVersion -ne [version]"$Version.0") { throw "Binary version $binaryVersion does not match requested package version $Version." }

[xml]$manifest = [IO.File]::ReadAllText((Join-Path $PSScriptRoot 'AppxManifest.xml'))
$manifest.Package.Identity.Version = "$Version.0"
if ($Publisher) { $manifest.Package.Identity.Publisher = $Publisher }
if ($PackageName) { $manifest.Package.Identity.Name = $PackageName }
if ($PublisherDisplayName) { $manifest.Package.Properties.PublisherDisplayName = $PublisherDisplayName }
$assets = Join-Path $PSScriptRoot 'Assets'
foreach ($required in @('StoreLogo.png', 'Square150x150Logo.png', 'Square44x44Logo.png', 'Wide310x150Logo.png', 'SmallTile.png', 'LargeTile.png', 'SplashScreen.png')) {
    if (!(Test-Path -LiteralPath (Join-Path $assets $required))) { throw "Missing package asset: $required" }
}
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$stage = Join-Path $BuildDirectory ('msix-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage | Out-Null
try {
    Copy-Item -LiteralPath $executable -Destination (Join-Path $stage 'AzkarReminder.exe')
    Copy-Item -LiteralPath $assets -Destination (Join-Path $stage 'Assets') -Recurse
    Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\audio') -Destination (Join-Path $stage 'audio') -Recurse
    Copy-Item -LiteralPath (Join-Path $projectRoot 'assets\licenses') -Destination (Join-Path $stage 'licenses') -Recurse
    foreach ($notice in @('LICENSE', 'THIRD_PARTY_NOTICES.md')) { Copy-Item -LiteralPath (Join-Path $projectRoot $notice) -Destination $stage }
    $manifest.Save((Join-Path $stage 'AppxManifest.xml'))
    $packagePath = Join-Path $OutputDirectory "AzkarReminder-$Version.msix"
    Invoke-Checked $makeAppx @('pack', '/d', $stage, '/p', $packagePath, '/o')
    if ($SignForLocalTest) {
        if (!$CertificateThumbprint) { throw 'Local signing requires -CertificateThumbprint from a certificate with a private key matching the manifest Publisher.' }
        & (Join-Path $PSScriptRoot 'sign_msix.ps1') -PackagePath $packagePath -CertificateThumbprint $CertificateThumbprint
    }
    $digest = (Get-FileHash -LiteralPath $packagePath -Algorithm SHA256).Hash.ToLowerInvariant()
    [IO.File]::WriteAllText("$packagePath.sha256", "$digest  $([IO.Path]::GetFileName($packagePath))`n", [Text.UTF8Encoding]::new($false))
    Write-Host "Created $packagePath. Package installation and Store submission still require validation."
} finally {
    $resolvedStage = [IO.Path]::GetFullPath($stage)
    if (!$resolvedStage.StartsWith($BuildDirectory.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) { throw 'Refusing cleanup outside the build directory.' }
    Remove-Item -LiteralPath $resolvedStage -Recurse -Force
}
