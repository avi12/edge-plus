<#
.SYNOPSIS
  Cut an Edge+ release: the portable exe and the MSI on GitHub, and the update
  manifest every installed copy polls (docs\updates.json).

.DESCRIPTION
  In order:

    1. set the version in app\panel-tauri\src-tauri\Cargo.toml, its one home;
    2. build the window kit and the release exe that embeds it (rebuilding the
       embedded split DLL if its sources are newer) and copy the exe to
       out\release\EdgePlus-<version>.exe;
    3. build the MSI from that exe (tools\Build-Installer.ps1);
    4. commit `chore(release): <version>`, tag v<version> and push both;
    5. create the GitHub release with the exe and the MSI attached, its notes
       the highlights plus a link to every commit since the previous tag;
    6. write the manifest from both files (tools\Write-UpdateManifest.ps1) and
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
param(
    [Parameter(Mandatory)]
    [ValidatePattern('^\d+\.\d+\.\d+$')]
    [string] $Version,
    [Parameter(Mandatory)]
    [string] $NotesFile
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Build.psm1') -Force

$repo = Get-RepoRoot
$manifest = Get-CargoManifestPath
$lockfile = Join-Path (Split-Path $manifest -Parent) 'Cargo.lock'
$current = Get-ProductVersion
$highlights = (Get-Content (Resolve-Path $NotesFile) -Raw).Trim()

$isTreeClean = -not (git -C $repo status --porcelain --untracked-files=no)
if (-not $isTreeClean) {
    throw 'Commit or stash your changes first: a release is built from what is committed.'
}
$isNewer = [version]$Version -gt [version]$current
if (-not $isNewer) {
    throw "$Version must be newer than the current $current."
}
$previousTag = git -C $repo describe --tags --abbrev=0
$asset = Get-ReleaseAsset $Version

Write-Step "Setting the version to $Version"
Set-ProductVersion $Version

Write-Step 'Building the exe'
$built = Build-ReleaseExe
$releaseFolder = Join-Path $repo 'out\release'
New-Item -ItemType Directory -Force $releaseFolder | Out-Null
$exe = Join-Path $releaseFolder $asset.Exe
Copy-Item $built $exe -Force

$installer = & (Join-Path $PSScriptRoot 'Build-Installer.ps1') -Exe $exe

Write-Step "Committing and tagging $($asset.Tag)"
Invoke-Checked 'git add' { git -C $repo add $manifest $lockfile }
Invoke-Checked 'git commit' { git -C $repo commit -m "chore(release): $Version" }
Invoke-Checked 'git tag' { git -C $repo tag $asset.Tag }
Invoke-Checked 'git push' { git -C $repo push origin main $asset.Tag }

Write-Step 'Creating the GitHub release'
$notes = Join-Path $releaseFolder "notes-$Version.md"
$allChanges = "https://github.com/$($asset.Repository)/compare/$previousTag...$($asset.Tag)"
Set-Content $notes "$highlights`n`n**All changes:** $allChanges`n" -Encoding utf8NoBOM
Invoke-Checked 'gh release create' {
    gh release create $asset.Tag $exe $installer --repo $asset.Repository `
        --title "Edge+ $Version" --notes-file $notes
}

Write-Step 'Publishing the update manifest'
& (Join-Path $PSScriptRoot 'Write-UpdateManifest.ps1') -Exe $exe -Installer $installer
Invoke-Checked 'git add' { git -C $repo add docs/updates.json }
Invoke-Checked 'git commit' {
    git -C $repo commit -m "chore(release): update manifest for $Version"
}
Invoke-Checked 'git push' { git -C $repo push origin main }

Write-Host "Released $($asset.Tag): https://github.com/$($asset.Repository)/releases/tag/$($asset.Tag)"
