<#
.SYNOPSIS
  Build the MSI that installs a published edgeplus.exe (app\installer).

.DESCRIPTION
  Wraps the exe it is given, so a release's MSI and its portable exe are the
  same bytes, and writes EdgePlus-<version>-setup.msi beside that exe. The
  version is Directory.Build.props', and the exe must carry the same one.
  tools\Release.ps1 runs it for every release.

.PARAMETER Exe
  The published exe the MSI installs.

.EXAMPLE
  pwsh tools\Build-Installer.ps1 -Exe out\release\EdgePlus-0.0.2.exe
#>
param(
    [Parameter(Mandatory)]
    [string] $Exe
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Build.psm1') -Force

$version = Get-ProductVersion
$exePath = (Resolve-Path $Exe).Path
$builtVersion = (Get-Item $exePath).VersionInfo.ProductVersion
if ($builtVersion -ne $version) {
    throw "$exePath is version $builtVersion, but Directory.Build.props says $version."
}

$project = Join-Path (Get-RepoRoot) 'app\installer'
$output = Join-Path $project 'bin\x64\Release'
$name = (Get-ReleaseAsset $version).Installer
Write-Step "Building the MSI for Edge+ $version"
Invoke-Checked 'dotnet build app\installer' {
    dotnet build $project -c Release --no-incremental -p:ExePath=$exePath -p:OutputPath=$output\ `
        -p:OutputName=$([IO.Path]::GetFileNameWithoutExtension($name)) -nologo -v:q |
        Out-Host
}

$installer = Join-Path (Split-Path $exePath -Parent) $name
Copy-Item (Join-Path $output $name) $installer -Force
Write-Host "wrote $installer"
return $installer
