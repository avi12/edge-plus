<#
.SYNOPSIS
  Pack a release's edgeplus.exe as the MSIX package users install.

.DESCRIPTION
  Lays out the exe, the startup launcher (src-tauri\launcher) and the logos
  drawn from icons\icon.ico beside app\package\AppxManifest.xml, stamps the
  manifest with Cargo.toml's version, packs it with makeappx and signs it with
  the code-signing certificate named by the manifest's publisher. Writes
  EdgePlus-<version>.msix beside the exe, and EdgePlus.cer, the certificate's
  public half a user trusts once so Windows accepts the package.
  tools\Release.ps1 runs it for every release.

.PARAMETER Exe
  The release exe the package carries.

.EXAMPLE
  pwsh tools\Build-Msix.ps1 -Exe out\release\edgeplus.exe
#>
using module .\EdgePlus.Build.psm1

param(
    [Parameter(Mandatory)]
    [string] $Exe
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Build.psm1') -Force
Add-Type -AssemblyName System.Drawing

[string] $timestampServer = 'http://timestamp.digicert.com'

class Logo {
    [string] $FileName
    [int] $Size
}

# The manifest names each file; the sizes are the ones the shell asks for.
function Get-Logos {
    [OutputType([Logo[]])]
    param()
    return [Logo[]]@(
        [Logo]@{ FileName = 'StoreLogo.png'; Size = 50 }
        [Logo]@{ FileName = 'Square44x44Logo.png'; Size = 44 }
        [Logo]@{ FileName = 'Square150x150Logo.png'; Size = 150 }
    )
}

function Build-Launcher {
    [OutputType([string])]
    param(
        [Parameter(Mandatory)]
        [string] $tauriFolder
    )
    Write-Step 'Building the startup launcher'
    Invoke-Checked 'cargo build (launcher)' {
        cargo build --release --manifest-path (Join-Path $tauriFolder 'Cargo.toml') -p edgeplus-launcher | Out-Host
    }
    return Join-Path $tauriFolder 'target\release\edgeplus-launcher.exe'
}

function Save-ResizedPng {
    [OutputType([void])]
    param(
        [Parameter(Mandatory)]
        [System.Drawing.Bitmap] $source,
        [Parameter(Mandatory)]
        [int] $size,
        [Parameter(Mandatory)]
        [string] $path
    )
    [System.Drawing.Bitmap] $bitmap = New-Object System.Drawing.Bitmap $size, $size
    [System.Drawing.Graphics] $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.InterpolationMode = 'HighQualityBicubic'
    $graphics.DrawImage($source, 0, 0, $size, $size)
    $graphics.Dispose()
    $bitmap.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
}

function Write-Logos {
    [OutputType([void])]
    param(
        [Parameter(Mandatory)]
        [string] $iconPath,
        [Parameter(Mandatory)]
        [string] $assetsFolder
    )
    Write-Step 'Drawing the logos'
    [System.Drawing.Icon] $icon = New-Object System.Drawing.Icon $iconPath, 256, 256
    [System.Drawing.Bitmap] $source = $icon.ToBitmap()
    foreach ($logo in Get-Logos) {
        Save-ResizedPng $source $logo.Size (Join-Path $assetsFolder $logo.FileName)
    }
    $source.Dispose()
    $icon.Dispose()
}

function New-Layout {
    [OutputType([void])]
    param(
        [Parameter(Mandatory)]
        [string] $layoutFolder,
        [Parameter(Mandatory)]
        [string] $exePath,
        [Parameter(Mandatory)]
        [string] $launcherPath,
        [Parameter(Mandatory)]
        [string] $iconPath
    )
    [string] $assetsFolder = Join-Path $layoutFolder 'assets'
    New-Item -ItemType Directory -Force $assetsFolder | Out-Null
    Copy-Item $exePath (Join-Path $layoutFolder 'edgeplus.exe')
    Copy-Item $launcherPath $layoutFolder
    Write-Logos $iconPath $assetsFolder
}

# Stamps the layout's manifest with the version and returns its publisher.
function Write-Manifest {
    [OutputType([string])]
    param(
        [Parameter(Mandatory)]
        [string] $layoutFolder,
        [Parameter(Mandatory)]
        [string] $version
    )
    [string] $manifestPath = Join-Path $layoutFolder 'AppxManifest.xml'
    Copy-Item (Join-Path (Get-RepoRoot) 'app\package\AppxManifest.xml') $manifestPath
    [xml] $document = Get-Content $manifestPath -Raw
    $document.Package.Identity.Version = "$version.0"
    $document.Save($manifestPath)
    return $document.Package.Identity.Publisher
}

function Find-SigningCertificate {
    [OutputType([System.Security.Cryptography.X509Certificates.X509Certificate2])]
    param(
        [Parameter(Mandatory)]
        [string] $publisher
    )
    $certificate = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert |
        Where-Object { $_.Subject -eq $publisher -and $_.NotAfter -gt (Get-Date) } |
        Sort-Object NotAfter -Descending | Select-Object -First 1
    if (-not $certificate) {
        throw "No valid code-signing certificate '$publisher' in Cert:\CurrentUser\My (the manifest's publisher)."
    }
    return $certificate
}

function New-SignedPackage {
    [OutputType([void])]
    param(
        [Parameter(Mandatory)]
        [string] $layoutFolder,
        [Parameter(Mandatory)]
        [string] $package,
        [Parameter(Mandatory)]
        [System.Security.Cryptography.X509Certificates.X509Certificate2] $certificate
    )
    [string] $makeappx = Find-SdkTool 'makeappx'
    [string] $signtool = Find-SdkTool 'signtool'
    Write-Step "Packing and signing $(Split-Path $package -Leaf)"
    Invoke-Checked 'makeappx pack' { & $makeappx pack /d $layoutFolder /p $package /o | Out-Host }
    Invoke-Checked 'signtool sign' {
        & $signtool sign /fd SHA256 /sha1 $certificate.Thumbprint /tr $timestampServer /td SHA256 $package | Out-Host
    }
}

[string] $repo = Get-RepoRoot
[string] $version = Get-ProductVersion
[string] $exePath = (Resolve-Path $Exe).Path
Assert-ExeVersion $exePath $version
[ReleaseAsset] $asset = Get-ReleaseAsset $version

[string] $tauriFolder = Join-Path $repo 'app\panel-tauri\src-tauri'
[string] $launcherPath = Build-Launcher $tauriFolder

[string] $stagingFolder = Join-Path $repo 'out\msix'
Remove-Item -Recurse -Force $stagingFolder -ErrorAction SilentlyContinue
[string] $layoutFolder = Join-Path $stagingFolder 'layout'
New-Layout $layoutFolder $exePath $launcherPath (Join-Path $tauriFolder 'icons\icon.ico')
[string] $publisher = Write-Manifest $layoutFolder $version
$certificate = Find-SigningCertificate $publisher

[string] $outputFolder = Split-Path $exePath -Parent
[string] $package = Join-Path $outputFolder $asset.Package
New-SignedPackage $layoutFolder $package $certificate

Export-Certificate -Cert $certificate -FilePath (Join-Path $outputFolder $asset.Certificate) | Out-Null
Remove-Item -Recurse -Force $stagingFolder

Write-Host "wrote $package"
return $package
