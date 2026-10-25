# The plumbing the repo's PowerShell tools share.

Set-StrictMode -Version Latest

function Get-RepoRoot {
    return Split-Path -Parent $PSScriptRoot
}

# What actually builds the C++ tree, and so the only description of how it is
# compiled.
function Get-BuildScriptPath {
    return Join-Path (Get-RepoRoot) 'app\native-split\edgeplus-split.buildscript'
}

# A tool that needs a compiler flag reads it from the buildscript instead of
# keeping a copy that drifts. The format is INI-shaped: `[section]`, then
# `key = value`.
function Get-BuildSetting([string] $section, [string] $key) {
    $isInSection = $false
    foreach ($line in Get-Content (Get-BuildScriptPath)) {
        $text = $line.Trim()
        if (-not $text -or $text.StartsWith('#')) {
            continue
        }
        if ($text -match '^\[(.+)\]$') {
            $isInSection = $Matches[1] -eq $section
            continue
        }
        if ($isInSection -and $text -match "^$([regex]::Escape($key))\s*(\[[^\]]*\])?\s*=\s*(.+)$") {
            return $Matches[2].Trim()
        }
    }
    throw "edgeplus-split.buildscript has no '$key' under [$section]."
}

# Asked of vswhere: the path carries the Visual Studio version and edition, so a
# literal one goes stale. Build Tools is hidden from vswhere's default filter, so
# every product is named.
function Get-VcVarsPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe is not at $vswhere; install Visual Studio or the Build Tools."
    }
    $installation = & $vswhere -products `
        Microsoft.VisualStudio.Product.BuildTools `
        Microsoft.VisualStudio.Product.Community `
        Microsoft.VisualStudio.Product.Professional `
        Microsoft.VisualStudio.Product.Enterprise `
        -latest -property installationPath | Select-Object -First 1
    if (-not $installation) {
        throw 'No Visual Studio or Build Tools installation found.'
    }
    $vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) {
        throw "$installation has no C++ toolset (no $vcvars). Install the Desktop C++ workload."
    }
    return $vcvars
}

function Invoke-Checked([string] $what, [scriptblock] $command) {
    & $command
    if ($LASTEXITCODE -ne 0) {
        throw "$what failed with exit code $LASTEXITCODE"
    }
}

function Write-Step([string] $text) {
    Write-Host "==> $text" -ForegroundColor Cyan
}

# The core's Cargo.toml: the one place the version is written (the exe's file
# version, the window's footer and the update check all read it).
function Get-CargoManifestPath {
    return Join-Path (Get-RepoRoot) 'app\panel-tauri\src-tauri\Cargo.toml'
}

# The [package] table's version, the first `version = "..."` in the file.
function Get-ProductVersion {
    $manifest = Get-CargoManifestPath
    $line = Get-Content $manifest | Where-Object { $_ -match '^version\s*=\s*"([^"]+)"' } |
        Select-Object -First 1
    if (-not $line) {
        throw "No version in $manifest."
    }
    return ([regex]::Match($line, '"([^"]+)"')).Groups[1].Value
}

function Set-ProductVersion([string] $version) {
    $manifest = Get-CargoManifestPath
    $text = Get-Content $manifest -Raw
    $text = ([regex]'(?m)^version\s*=\s*"[^"]*"').Replace($text, "version = `"$version`"", 1)
    Set-Content $manifest $text -Encoding utf8NoBOM -NoNewline
}

# The release build: the window kit (pnpm build:window), then the core in
# release, which embeds it. Returns the exe.
function Build-ReleaseExe {
    $panel = Join-Path (Get-RepoRoot) 'app\panel-tauri'
    Invoke-Checked 'pnpm install' { pnpm --dir $panel install --frozen-lockfile }
    Invoke-Checked 'pnpm restore' { pnpm --dir $panel restore }
    Invoke-Checked 'pnpm build' { pnpm --dir $panel build }
    return Join-Path $panel 'src-tauri\target\release\edgeplus.exe'
}

# A release's files on GitHub, as the release attaches them and the update
# manifest names them.
function Get-ReleaseAsset([string] $version) {
    $repository = 'avi12/edge-plus'
    return [pscustomobject]@{
        Repository = $repository
        Tag        = "v$version"
        Exe        = "EdgePlus-$version.exe"
        Installer  = "EdgePlus-$version-setup.msi"
        Url        = "https://github.com/$repository/releases/download/v$version"
    }
}

Export-ModuleMember -Function Get-RepoRoot, Get-BuildScriptPath, Get-BuildSetting, Get-VcVarsPath,
    Invoke-Checked, Write-Step, Get-CargoManifestPath, Get-ProductVersion, Set-ProductVersion,
    Build-ReleaseExe, Get-ReleaseAsset
