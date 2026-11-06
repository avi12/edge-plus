<#
.SYNOPSIS
  Write docs/updates.json, the manifest installed copies of Edge+ poll to find
  out whether a newer release exists.

.DESCRIPTION
  Names the version in app\panel-tauri\src-tauri\Cargo.toml, where that
  release's MSIX is (the asset EdgePlus-<version>.msix on the GitHub release)
  and its SHA-256, which an installed copy checks its download against.
  tools\Release.ps1 runs it after the package is attached to the release and
  pushes the result to main: until then installed copies see no update, so the
  manifest never points at a file that is not there yet. The package sits under
  its own `msix` key: a copy that predates packaging reads a top-level `url` as
  an exe, so it must find none.

.PARAMETER Package
  The MSIX attached to the release. Its manifest version must match the version
  in Cargo.toml, so a stale build cannot be published as a new release.

.EXAMPLE
  pwsh tools\Write-UpdateManifest.ps1 -Package EdgePlus-0.2.0.msix
#>
using module .\EdgePlus.Build.psm1

param(
    [Parameter(Mandatory)]
    [string] $Package
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Build.psm1') -Force

class MsixEntry {
    [string] $url
    [string] $sha256
}

class UpdateManifest {
    [string] $version
    [MsixEntry] $msix
}

function New-UpdateManifest {
    [OutputType([UpdateManifest])]
    param(
        [Parameter(Mandatory)]
        [string] $version,
        [Parameter(Mandatory)]
        [ReleaseAsset] $asset,
        [Parameter(Mandatory)]
        [string] $packagePath
    )
    return [UpdateManifest]@{
        version = $version
        msix    = [MsixEntry]@{
            url    = "$($asset.Url)/$($asset.Package)"
            sha256 = (Get-FileHash $packagePath -Algorithm SHA256).Hash
        }
    }
}

[string] $version = Get-ProductVersion
[string] $packagePath = (Resolve-Path $Package).Path
[ReleaseAsset] $asset = Get-ReleaseAsset $version
[bool] $isExpectedPackage = (Split-Path $packagePath -Leaf) -eq $asset.Package
if (-not $isExpectedPackage) {
    throw "$packagePath is not $($asset.Package), the package of Cargo.toml's version $version."
}

[UpdateManifest] $manifest = New-UpdateManifest $version $asset $packagePath
[string] $target = Join-Path (Get-RepoRoot) 'docs\updates.json'
($manifest | ConvertTo-Json) + "`n" | Set-Content $target -Encoding utf8NoBOM -NoNewline
Write-Host "wrote $target"
Get-Content $target
