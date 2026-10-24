<#
.SYNOPSIS
  Write docs/updates.json, the manifest installed copies of Edge+ poll to find
  out whether a newer release exists.

.DESCRIPTION
  Names the version in app\panel-winui\Directory.Build.props, where that
  release's exe is (the asset EdgePlus-<version>.exe on the GitHub release) and
  the exe's SHA-256, which an installed copy checks its download against. Run it
  after the exe is attached to the release and push the result to main: until
  then installed copies see no update, so the manifest never points at an exe
  that is not there yet.

.PARAMETER Exe
  The exe attached to the release. Its file version must match the version in
  Directory.Build.props, so a stale build cannot be published as a new release.

.EXAMPLE
  pwsh tools\Write-UpdateManifest.ps1 -Exe EdgePlus-0.0.2.exe
#>
param(
    [Parameter(Mandatory)]
    [string] $Exe
)

$ErrorActionPreference = 'Stop'

$repo = Split-Path $PSScriptRoot -Parent
$props = [xml](Get-Content (Join-Path $repo 'app\panel-winui\Directory.Build.props') -Raw)
$version = ($props.Project.PropertyGroup | Where-Object { $_.Version } | Select-Object -First 1).Version

$exePath = (Resolve-Path $Exe).Path
$builtVersion = (Get-Item $exePath).VersionInfo.ProductVersion
if ($builtVersion -ne $version) {
    throw "$exePath is version $builtVersion, but Directory.Build.props says $version. Build the release first."
}

$manifest = [ordered]@{
    version = $version
    url     = "https://github.com/avi12/edge-plus/releases/download/v$version/EdgePlus-$version.exe"
    sha256  = (Get-FileHash $exePath -Algorithm SHA256).Hash
}
$target = Join-Path $repo 'docs\updates.json'
($manifest | ConvertTo-Json) + "`n" | Set-Content $target -Encoding utf8NoBOM -NoNewline
Write-Host "wrote $target"
Get-Content $target
