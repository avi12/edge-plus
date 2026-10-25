<#
.SYNOPSIS
  Write docs/updates.json, the manifest installed copies of Edge+ poll to find
  out whether a newer release exists.

.DESCRIPTION
  Names the version in app\panel-winui\Directory.Build.props, where that
  release's exe and MSI are (the assets EdgePlus-<version>.exe and
  EdgePlus-<version>-setup.msi on the GitHub release) and each one's SHA-256,
  which an installed copy checks its download against: a portable copy fetches
  the exe, a copy the MSI installed fetches the MSI. tools\Release.ps1 runs it
  after both are attached to the release and pushes the result to main: until
  then installed copies see no update, so the manifest never points at a file
  that is not there yet.

.PARAMETER Exe
  The exe attached to the release. Its file version must match the version in
  Directory.Build.props, so a stale build cannot be published as a new release.

.PARAMETER Installer
  The MSI attached to the release, built from that exe.

.EXAMPLE
  pwsh tools\Write-UpdateManifest.ps1 -Exe EdgePlus-0.0.2.exe -Installer EdgePlus-0.0.2-setup.msi
#>
param(
    [Parameter(Mandatory)]
    [string] $Exe,
    [Parameter(Mandatory)]
    [string] $Installer
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Build.psm1') -Force

$version = Get-ProductVersion
$exePath = (Resolve-Path $Exe).Path
$builtVersion = (Get-Item $exePath).VersionInfo.ProductVersion
if ($builtVersion -ne $version) {
    throw "$exePath is version $builtVersion, but Directory.Build.props says $version. Build the release first."
}

$asset = Get-ReleaseAsset $version
$manifest = [ordered]@{
    version   = $version
    url       = "$($asset.Url)/$($asset.Exe)"
    sha256    = (Get-FileHash $exePath -Algorithm SHA256).Hash
    installer = [ordered]@{
        url    = "$($asset.Url)/$($asset.Installer)"
        sha256 = (Get-FileHash (Resolve-Path $Installer).Path -Algorithm SHA256).Hash
    }
}
$target = Join-Path (Get-RepoRoot) 'docs\updates.json'
($manifest | ConvertTo-Json) + "`n" | Set-Content $target -Encoding utf8NoBOM -NoNewline
Write-Host "wrote $target"
Get-Content $target
