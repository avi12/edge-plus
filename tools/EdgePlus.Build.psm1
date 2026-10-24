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

# Directory.Build.props is the one place the version is written.
function Get-ProductVersion {
    $props = Join-Path (Get-RepoRoot) 'app\panel-winui\Directory.Build.props'
    $version = ([xml](Get-Content $props -Raw)).Project.PropertyGroup.Version |
        Where-Object { $_ } | Select-Object -First 1
    if (-not $version) {
        throw "No <Version> in $props."
    }
    return $version
}

Export-ModuleMember -Function Get-RepoRoot, Get-BuildScriptPath, Get-BuildSetting, Get-VcVarsPath,
    Invoke-Checked, Write-Step, Get-ProductVersion
