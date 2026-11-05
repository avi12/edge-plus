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
param(
    [Parameter(Mandatory)]
    [string] $Exe
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Build.psm1') -Force
Add-Type -AssemblyName System.Drawing

$repo = Get-RepoRoot
$version = Get-ProductVersion
$exePath = (Resolve-Path $Exe).Path
$built = (Get-Item $exePath).VersionInfo
if ($built.ProductVersion -ne $version) {
    throw "$exePath is version $($built.ProductVersion), but Cargo.toml says $version."
}
$asset = Get-ReleaseAsset $version

# Logos: the manifest names each file, the sizes are the ones the shell asks for.
$logos = [ordered]@{
    'StoreLogo.png'         = 50
    'Square44x44Logo.png'   = 44
    'Square150x150Logo.png' = 150
}

$tauri = Join-Path $repo 'app\panel-tauri\src-tauri'
Write-Step 'Building the startup launcher'
Invoke-Checked 'cargo build (launcher)' {
    cargo build --release --manifest-path (Join-Path $tauri 'Cargo.toml') -p edgeplus-launcher | Out-Host
}
$launcher = Join-Path $tauri 'target\release\edgeplus-launcher.exe'

$staging = Join-Path $repo 'out\msix'
Remove-Item -Recurse -Force $staging -ErrorAction SilentlyContinue
$layout = Join-Path $staging 'layout'
$assets = Join-Path $layout 'assets'
New-Item -ItemType Directory -Force $assets | Out-Null
Copy-Item $exePath (Join-Path $layout 'edgeplus.exe')
Copy-Item $launcher $layout

Write-Step 'Drawing the logos'
$icon = New-Object System.Drawing.Icon (Join-Path $tauri 'icons\icon.ico'), 256, 256
$source = $icon.ToBitmap()
foreach ($name in $logos.Keys) {
    $size = $logos[$name]
    $bitmap = New-Object System.Drawing.Bitmap $size, $size
    $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
    $graphics.InterpolationMode = 'HighQualityBicubic'
    $graphics.DrawImage($source, 0, 0, $size, $size)
    $graphics.Dispose()
    $bitmap.Save((Join-Path $assets $name), [System.Drawing.Imaging.ImageFormat]::Png)
    $bitmap.Dispose()
}
$source.Dispose()
$icon.Dispose()

$manifestPath = Join-Path $layout 'AppxManifest.xml'
Copy-Item (Join-Path $repo 'app\package\AppxManifest.xml') $manifestPath
$document = [xml](Get-Content $manifestPath -Raw)
$document.Package.Identity.Version = "$version.0"
$document.Save($manifestPath)
$publisher = $document.Package.Identity.Publisher

$certificate = Get-ChildItem Cert:\CurrentUser\My -CodeSigningCert |
    Where-Object { $_.Subject -eq $publisher -and $_.NotAfter -gt (Get-Date) } |
    Sort-Object NotAfter -Descending | Select-Object -First 1
if (-not $certificate) {
    throw "No valid code-signing certificate '$publisher' in Cert:\CurrentUser\My (the manifest's publisher)."
}

$sdk = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\10.*\x64" -Directory |
    Sort-Object { [version]$_.Parent.Name } -Descending |
    Select-Object -First 1
$makeappx = Join-Path $sdk.FullName 'makeappx.exe'
$signtool = Join-Path $sdk.FullName 'signtool.exe'

$package = Join-Path (Split-Path $exePath -Parent) $asset.Package
Write-Step "Packing and signing $($asset.Package)"
Invoke-Checked 'makeappx pack' { & $makeappx pack /d $layout /p $package /o | Out-Host }
Invoke-Checked 'signtool sign' {
    & $signtool sign /fd SHA256 /sha1 $certificate.Thumbprint /tr http://timestamp.digicert.com /td SHA256 $package | Out-Host
}

$certificateFile = Join-Path (Split-Path $exePath -Parent) $asset.Certificate
Export-Certificate -Cert $certificate -FilePath $certificateFile | Out-Null
Remove-Item -Recurse -Force $staging

Write-Host "wrote $package"
return $package
