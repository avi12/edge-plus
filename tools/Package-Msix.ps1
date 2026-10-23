<#
.SYNOPSIS
  Build the Microsoft Store package: dist\EdgePlus-<version>-x64.msix.

.DESCRIPTION
  Left UNSIGNED, which is what the Store wants: Partner Center signs it on
  upload.

  The identity in app\panel-winui\Package.appxmanifest is a placeholder until
  the product name is reserved in Partner Center (see docs\STORE-SUBMISSION.md);
  the package builds and packs with it, and Partner Center refuses it.

  Known gap: nothing has been installed from this package and run end to end.

.PARAMETER CertificateThumbprint
  Sign with this certificate from Cert:\CurrentUser\My, only to sideload for
  testing.

.EXAMPLE
  pwsh tools\Package-Msix.ps1
#>
[CmdletBinding()]
param(
    [string] $CertificateThumbprint
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'FixEdge.Build.psm1') -Force
Add-Type -AssemblyName System.Drawing

$repo = Get-RepoRoot
$app = Join-Path $repo 'app\panel-winui'
$dist = Join-Path $repo 'dist'
$layout = Join-Path $dist 'msix'

# makeappx and signtool come from the Windows SDK build tools the panel already
# restores, so packaging needs no separate SDK install.
function Find-SdkTool([string] $name) {
    $packages = Join-Path $env:USERPROFILE '.nuget\packages\microsoft.windows.sdk.buildtools'
    $tool = Get-ChildItem $packages -Recurse -Filter $name -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1
    if (-not $tool) {
        throw "$name not found under $packages. Run 'dotnet restore app\panel-winui' first."
    }
    return $tool.FullName
}

# Scaled from the icon's largest frame.
$logoSizes = @{ 'Square44x44Logo.png' = 44; 'Square150x150Logo.png' = 150; 'StoreLogo.png' = 50 }
function New-MsixLogos([string] $iconPath, [string] $targetDir) {
    New-Item -ItemType Directory -Force $targetDir | Out-Null
    $icon = [System.Drawing.Icon]::new($iconPath, 256, 256)
    try {
        $source = $icon.ToBitmap()
        try {
            foreach ($file in $logoSizes.Keys) {
                $size = $logoSizes[$file]
                $scaled = [System.Drawing.Bitmap]::new($size, $size)
                $graphics = [System.Drawing.Graphics]::FromImage($scaled)
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.DrawImage($source, 0, 0, $size, $size)
                $graphics.Dispose()
                $scaled.Save((Join-Path $targetDir $file), [System.Drawing.Imaging.ImageFormat]::Png)
                $scaled.Dispose()
            }
        } finally { $source.Dispose() }
    } finally { $icon.Dispose() }
}

Remove-Item -Recurse -Force $dist -ErrorAction SilentlyContinue

# WindowsPackageType=MSIX is not cosmetic: it makes the resource index a packaged
# one. PublishSingleFile is not supported for a packaged app.
Invoke-Checked 'dotnet publish (msix layout)' {
    dotnet publish $app -c Release -p:Platform=x64 -o $layout --nologo -v quiet `
        -p:WindowsPackageType=MSIX -p:PublishSingleFile=false
}

New-MsixLogos -iconPath (Join-Path $app 'Assets\edgeplus.ico') -targetDir (Join-Path $layout 'Assets')

# The exe's version, so the two cannot disagree; the Store requires the fourth
# part to be 0.
$version = (Get-Item (Join-Path $layout 'edgeplus.exe')).VersionInfo.ProductVersion
if ($version -notmatch '^\d+\.\d+\.\d+') {
    throw "Expected a version that starts with three numbers, got '$version'"
}
$parts = ($version -split '[.+-]')[0..2]
$manifestPath = Join-Path $layout 'AppxManifest.xml'
if (-not (Test-Path $manifestPath)) {
    Copy-Item (Join-Path $app 'Package.appxmanifest') $manifestPath
}
$manifest = [xml](Get-Content $manifestPath -Raw)
$manifest.Package.Identity.Version = ($parts -join '.') + '.0'
$manifest.Save($manifestPath)

$msix = Join-Path $dist ('EdgePlus-{0}-x64.msix' -f ($parts -join '.'))
Invoke-Checked 'makeappx pack' {
    & (Find-SdkTool 'makeappx.exe') pack /d $layout /p $msix /o
}

if ($CertificateThumbprint) {
    Invoke-Checked 'signtool' {
        & (Find-SdkTool 'signtool.exe') sign /fd SHA256 /sha1 $CertificateThumbprint `
            /tr http://timestamp.digicert.com /td SHA256 $msix
    }
}

$hash = (Get-FileHash $msix -Algorithm SHA256).Hash
"$hash  $([IO.Path]::GetFileName($msix))" | Set-Content "$msix.sha256" -Encoding ASCII
'{0}  {1:N1} MB  {2}' -f [IO.Path]::GetFileName($msix), ((Get-Item $msix).Length / 1MB), $hash.Substring(0, 16)
