# FixEdge.Build.psm1 — the plumbing the repo's PowerShell tools share, so each
# script holds only its own concern. Modelled on avi/any-stt's AnyStt.Build.psm1.
#
# Import from a sibling script with:
#   Import-Module (Join-Path $PSScriptRoot 'FixEdge.Build.psm1') -Force

Set-StrictMode -Version Latest

# The repository root, from this module's own home in tools\ — so a script does
# not re-derive it and they cannot disagree.
function Get-RepoRoot {
    return Split-Path -Parent $PSScriptRoot
}

# The native split's buildscript: what actually builds the C++ tree, and so the
# only description of how it is compiled.
function Get-BuildScriptPath {
    return Join-Path (Get-RepoRoot) 'app\native-split\fixedge-split.buildscript'
}

# One setting out of the buildscript. A tool that needs a compiler flag — the
# standard, the defines, the include path — reads it from there instead of
# keeping a copy that drifts silently. The format is INI-shaped: `[section]`,
# then `key = value`.
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
    throw "fixedge-split.buildscript has no '$key' under [$section]."
}

# Where vcvars64.bat is, asked of vswhere rather than spelled out: the path
# carries the Visual Studio version and edition, so a literal one goes stale.
# Build Tools is hidden from vswhere's default filter, so every product is named.
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

# The product version: <Version> in Directory.Build.props, the one place it is
# written.
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
