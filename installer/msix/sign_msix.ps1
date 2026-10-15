[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$PackagePath,
    [Parameter(Mandatory)][string]$CertificateThumbprint
)
$ErrorActionPreference = 'Stop'
$PackagePath = (Resolve-Path -LiteralPath $PackagePath).Path
$certificate = Get-Item -LiteralPath "Cert:\CurrentUser\My\$CertificateThumbprint"
if (!$certificate.HasPrivateKey) { throw 'The signing certificate has no private key.' }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::OpenRead($PackagePath)
try {
    $reader = [IO.StreamReader]::new($archive.GetEntry('AppxManifest.xml').Open())
    try { [xml]$manifest = $reader.ReadToEnd() } finally { $reader.Dispose() }
} finally { $archive.Dispose() }
if ($certificate.Subject -ne $manifest.Package.Identity.Publisher) { throw 'Certificate Subject does not match the package Publisher.' }
$signtool = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\*\x64\signtool.exe" -ErrorAction SilentlyContinue |
    Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
if (!$signtool) { throw 'Install the Windows SDK: signtool.exe was not found.' }
& $signtool sign /fd SHA256 /sha1 $certificate.Thumbprint $PackagePath
if ($LASTEXITCODE -ne 0) { throw "MSIX signing failed with exit code $LASTEXITCODE" }
$digest = (Get-FileHash -LiteralPath $PackagePath -Algorithm SHA256).Hash.ToLowerInvariant()
[IO.File]::WriteAllText("$PackagePath.sha256", "$digest  $([IO.Path]::GetFileName($PackagePath))`n", [Text.UTF8Encoding]::new($false))
Write-Host "Signed $PackagePath. Trust and installation were not changed."
