<#
.SYNOPSIS
  Cut an Edge+ release: bump the version, push it and create the GitHub
  release; the workflow builds and attaches the files.

.DESCRIPTION
  In order:

    1. set the version in app\panel\src-tauri\Cargo.toml, its one home;
    2. commit `chore(release): <version>` and push main;
    3. create the GitHub release v<version> from that commit, its notes the
       highlights plus a link to every commit since the previous tag.

  Creating the release creates the tag, and the tag starts
  .github\workflows\release.yml, which builds the exe, packs the Squirrel-style
  zip and releases.json (winui-svelte's createRelease) and attaches them with
  EdgePlus-Setup.exe. Installed copies read releases.json from the latest
  release, so they see the version once the workflow has attached it.

  Refuses a dirty tracked tree and a version that is not newer than the current
  one. Needs gh signed in.

.PARAMETER Version
  The new version, major.minor.patch.

.PARAMETER NotesFile
  The changelog highlights, written for users: plain sentences, no file names.

.EXAMPLE
  pwsh tools\Release.ps1 -Version 0.4.0 -NotesFile $env:TEMP\notes-0.4.0.md
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
[string] $repository = 'avi12/edge-plus'
[string] $tag = "v$Version"

[bool] $hasUncommittedChanges = [bool](git -C $repo status --porcelain --untracked-files=no)
if ($hasUncommittedChanges) {
    throw 'Commit or stash your changes first: a release is built from what is committed.'
}
[bool] $isNewer = [version]$Version -gt [version]$current
if (-not $isNewer) {
    throw "$Version must be newer than the current $current."
}
[string] $previousTag = git -C $repo describe --tags --abbrev=0

Write-Step "Setting the version to $Version"
Set-ProductVersion $Version
Invoke-Checked 'cargo check' {
    cargo check --manifest-path $manifest --quiet | Out-Host
}

Write-Step "Committing $Version"
Invoke-Checked 'git add' { git -C $repo add $manifest $lockfile }
Invoke-Checked 'git commit' { git -C $repo commit -m "chore(release): $Version" }
Invoke-Checked 'git push' { git -C $repo push origin main }

Write-Step "Creating the GitHub release $tag"
[string] $notes = Join-Path (Get-ReleaseFolder) "notes-$Version.md"
New-Item -ItemType Directory -Force (Split-Path $notes -Parent) | Out-Null
[string] $allChanges = "https://github.com/$repository/compare/$previousTag...$tag"
[string] $installLine = '**Install:** download ``EdgePlus-Setup.exe`` below and open it. It installs Edge+ for you, adds it to Start and to Add or remove programs, and updates itself.'
Set-Content $notes "$highlights`n`n$installLine`n`n**All changes:** $allChanges`n" -Encoding utf8NoBOM
Invoke-Checked 'gh release create' {
    gh release create $tag --repo $repository --target main --title "Edge+ $Version" --notes-file $notes
}

Write-Host "Released ${tag}: https://github.com/$repository/releases/tag/$tag (the workflow attaches the files)"
