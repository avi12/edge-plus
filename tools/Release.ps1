<#
.SYNOPSIS
  Cut an Edge+ release: the signed MSIX on GitHub, and the update manifest
  every installed copy polls (docs\updates.json).

.DESCRIPTION
  In order:

    1. set the version in app\panel-tauri\src-tauri\Cargo.toml, its one home;
    2. build the window kit and the release exe that embeds it (rebuilding the
       embedded split DLL if its sources are newer) and copy the exe to
       out\release\EdgePlus-<version>.exe;
    3. pack that exe as the signed MSIX (tools\Build-Msix.ps1), with the
       certificate a user trusts once to install it;
    4. commit `chore(release): <version>`, tag v<version> and push both;
    5. create the GitHub release with the MSIX and the certificate attached, its
       notes the highlights plus a link to every commit since the previous tag;
    6. write the manifest from the package (tools\Write-UpdateManifest.ps1) and
       push it - last, so an installed copy never sees a release whose files
       are not uploaded yet.

  Refuses a dirty tracked tree and a version that is not newer than the current
  one. Needs gh signed in.

.PARAMETER Version
  The new version, major.minor.patch.

.PARAMETER NotesFile
  The changelog highlights, written for users: plain sentences, no file names.

.EXAMPLE
  pwsh tools\Release.ps1 -Version 0.0.2 -NotesFile $env:TEMP\notes-0.0.2.md
#>
using module .\EdgePlus.Build.psm1

param(
    [Parameter(Mandatory)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string] $Version,
    [Parameter(Mandatory)]
    [string] $NotesFile
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Build.psm1') -Force

[string] $repo = Get-RepoRoot
[string] $manifest = Get-CargoManifestPath
[string] $lockfile = Join-Path (Split-Path $manifest -Parent) 'Cargo.lock'
[string] $current = Get-ProductVersion
[string] $highlights = (Get-Content (Resolve-Path $NotesFile) -Raw).Trim()

[bool] $isTreeClean = -not (git -C $repo status --porcelain --untracked-files=no)
if (-not $isTreeClean) {
    throw 'Commit or stash your changes first: a release is built from what is committed.'
}
[bool] $isNewer = [version]$Version -gt [version]$current
if (-not $isNewer) {
    throw "$Version must be newer than the current $current."
}
[string] $previousTag = git -C $repo describe --tags --abbrev=0
[ReleaseAsset] $asset = Get-ReleaseAsset $Version

Write-Step "Setting the version to $Version"
Set-ProductVersion $Version

Write-Step 'Building the exe'
[string] $built = Build-ReleaseExe
[string] $releaseFolder = Join-Path $repo 'out\release'
New-Item -ItemType Directory -Force $releaseFolder | Out-Null
[string] $exe = Join-Path $releaseFolder $asset.Exe
Copy-Item $built $exe -Force

[string] $package = & (Join-Path $PSScriptRoot 'Build-Msix.ps1') -Exe $exe
[string] $certificate = Join-Path $releaseFolder $asset.Certificate

Write-Step "Committing and tagging $($asset.Tag)"
Invoke-Checked 'git add' { git -C $repo add $manifest $lockfile }
Invoke-Checked 'git commit' { git -C $repo commit -m "chore(release): $Version" }
Invoke-Checked 'git tag' { git -C $repo tag $asset.Tag }
Invoke-Checked 'git push' { git -C $repo push origin main $asset.Tag }

Write-Step 'Creating the GitHub release'
[string] $notes = Join-Path $releaseFolder "notes-$Version.md"
[string] $allChanges = "https://github.com/$($asset.Repository)/compare/$previousTag...$($asset.Tag)"
Set-Content $notes "$highlights`n`n**All changes:** $allChanges`n" -Encoding utf8NoBOM
Invoke-Checked 'gh release create' {
    gh release create $asset.Tag $package $certificate --repo $asset.Repository `
        --title "Edge+ $Version" --notes-file $notes
}

Write-Step 'Publishing the update manifest'
& (Join-Path $PSScriptRoot 'Write-UpdateManifest.ps1') -Package $package
Invoke-Checked 'git add' { git -C $repo add docs/updates.json }
Invoke-Checked 'git commit' {
    git -C $repo commit -m "chore(release): update manifest for $Version"
}
Invoke-Checked 'git push' { git -C $repo push origin main }

Write-Host "Released $($asset.Tag): https://github.com/$($asset.Repository)/releases/tag/$($asset.Tag)"
