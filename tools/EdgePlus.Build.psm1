# The plumbing the repo's PowerShell tools share.

Set-StrictMode -Version Latest

class ReleaseAsset {
    [string] $Repository
    [string] $Tag
    [string] $Exe
    [string] $Package
    [string] $Certificate
    [string] $Url
}

function Get-RepoRoot {
    [OutputType([string])]
    param()
    return Split-Path -Parent $PSScriptRoot
}

# Where a release's exe, package and certificate are gathered before they are
# uploaded.
function Get-ReleaseFolder {
    [OutputType([string])]
    param()
    return Join-Path (Get-RepoRoot) 'out\release'
}

# What actually builds the C++ tree, and so the only description of how it is
# compiled.
function Get-BuildScriptPath {
    [OutputType([string])]
    param()
    return Join-Path (Get-RepoRoot) 'app\native-split\edgeplus-split.buildscript'
}

# A tool that needs a compiler flag reads it from the buildscript instead of
# keeping a copy that drifts. The format is INI-shaped: `[section]`, then
# `key = value`.
function Get-BuildSetting {
    [OutputType([string])]
    param(
        [Parameter(Mandatory)]
        [string] $section,
        [Parameter(Mandatory)]
        [string] $key
    )
    [bool] $isInSection = $false
    foreach ($line in Get-Content (Get-BuildScriptPath)) {
        [string] $text = $line.Trim()
        [bool] $isSkippable = -not $text -or $text.StartsWith('#')
        if ($isSkippable) {
            continue
        }
        if ($text -match '^\[(.+)\]$') {
            $isInSection = $Matches[1] -eq $section
            continue
        }
        [bool] $isRequestedSetting = $isInSection -and
            $text -match "^$([regex]::Escape($key))\s*(\[[^\]]*\])?\s*=\s*(.+)$"
        if ($isRequestedSetting) {
            return $Matches[2].Trim()
        }
    }
    throw "edgeplus-split.buildscript has no '$key' under [$section]."
}

# Asked of vswhere: the path carries the Visual Studio version and edition, so a
# literal one goes stale. Build Tools is hidden from vswhere's default filter, so
# every product is named.
function Get-VcVarsPath {
    [OutputType([string])]
    param()
    [string] $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path $vswhere)) {
        throw "vswhere.exe is not at $vswhere; install Visual Studio or the Build Tools."
    }
    [string] $installation = & $vswhere -products `
        Microsoft.VisualStudio.Product.BuildTools `
        Microsoft.VisualStudio.Product.Community `
        Microsoft.VisualStudio.Product.Professional `
        Microsoft.VisualStudio.Product.Enterprise `
        -latest -property installationPath | Select-Object -First 1
    if (-not $installation) {
        throw 'No Visual Studio or Build Tools installation found.'
    }
    [string] $vcvars = Join-Path $installation 'VC\Auxiliary\Build\vcvars64.bat'
    if (-not (Test-Path $vcvars)) {
        throw "$installation has no C++ toolset (no $vcvars). Install the Desktop C++ workload."
    }
    return $vcvars
}

# A tool of the newest installed Windows SDK (makeappx, signtool).
function Find-SdkTool {
    [OutputType([string])]
    param(
        [Parameter(Mandatory)]
        [string] $name
    )
    $newestSdk = Get-ChildItem "${env:ProgramFiles(x86)}\Windows Kits\10\bin\10.*\x64" -Directory |
        Sort-Object { [version]$_.Parent.Name } -Descending |
        Select-Object -First 1
    if (-not $newestSdk) {
        throw 'No Windows SDK found; install the Windows 10/11 SDK.'
    }
    [string] $tool = Join-Path $newestSdk.FullName "$name.exe"
    if (-not (Test-Path $tool)) {
        throw "$tool is missing; reinstall the Windows SDK."
    }
    return $tool
}

function Invoke-Checked {
    param(
        [Parameter(Mandatory)]
        [string] $what,
        [Parameter(Mandatory)]
        [scriptblock] $command
    )
    & $command
    if ($LASTEXITCODE -ne 0) {
        throw "$what failed with exit code $LASTEXITCODE"
    }
}

function Write-Step {
    [OutputType([void])]
    param(
        [Parameter(Mandatory)]
        [string] $text
    )
    Write-Host "==> $text" -ForegroundColor Cyan
}

# The core's Cargo.toml: the one place the version is written (the exe's file
# version, the window's footer and the update check all read it).
function Get-CargoManifestPath {
    [OutputType([string])]
    param()
    return Join-Path (Get-RepoRoot) 'app\panel-tauri\src-tauri\Cargo.toml'
}

# The [package] table's version, the first `version = "..."` in the file.
function Get-ProductVersion {
    [OutputType([string])]
    param()
    [string] $manifest = Get-CargoManifestPath
    [string] $line = Get-Content $manifest | Where-Object { $_ -match '^version\s*=\s*"([^"]+)"' } |
        Select-Object -First 1
    if (-not $line) {
        throw "No version in $manifest."
    }
    return ([regex]::Match($line, '"([^"]+)"')).Groups[1].Value
}

function Set-ProductVersion {
    [OutputType([void])]
    param(
        [Parameter(Mandatory)]
        [string] $version
    )
    [string] $manifest = Get-CargoManifestPath
    [string] $text = Get-Content $manifest -Raw
    $text = ([regex]'(?m)^version\s*=\s*"[^"]*"').Replace($text, "version = `"$version`"", 1)
    Set-Content $manifest $text -Encoding utf8NoBOM -NoNewline
}

# A stale build must not be packed or published as the new release.
function Assert-ExeVersion {
    [OutputType([void])]
    param(
        [Parameter(Mandatory)]
        [string] $exePath,
        [Parameter(Mandatory)]
        [string] $version
    )
    [string] $builtVersion = (Get-Item $exePath).VersionInfo.ProductVersion
    [bool] $isVersionCurrent = $builtVersion -eq $version
    if (-not $isVersionCurrent) {
        throw "$exePath is version $builtVersion, but Cargo.toml says $version."
    }
}

# The release build: the window kit (pnpm build:window), then the core in
# release, which embeds it. Returns the exe.
function Build-ReleaseExe {
    [OutputType([string])]
    param()
    [string] $panel = Join-Path (Get-RepoRoot) 'app\panel-tauri'
    # Their output goes to the console: left in the pipeline, a line such as
    # pnpm's "Scope: all 3 workspace projects" was returned along with the path.
    Invoke-Checked 'pnpm install' { pnpm --dir $panel install --frozen-lockfile } | Out-Host
    Invoke-Checked 'pnpm restore' { pnpm --dir $panel restore } | Out-Host
    Invoke-Checked 'pnpm build' { pnpm --dir $panel build } | Out-Host
    return Join-Path $panel 'src-tauri\target\release\edgeplus.exe'
}

# A release's files on GitHub, as the release attaches them (the package and the
# certificate that makes Windows accept it) and the update manifest names them.
# The exe is only the build the package carries.
function Get-ReleaseAsset {
    [OutputType([ReleaseAsset])]
    param(
        [Parameter(Mandatory)]
        [string] $version
    )
    [string] $repository = 'avi12/edge-plus'
    return [ReleaseAsset]@{
        Repository  = $repository
        Tag         = "v$version"
        Exe         = "EdgePlus-$version.exe"
        Package     = "EdgePlus-$version.msix"
        Certificate = 'EdgePlus.cer'
        Url         = "https://github.com/$repository/releases/download/v$version"
    }
}

Export-ModuleMember -Function Get-RepoRoot, Get-ReleaseFolder, Get-BuildScriptPath, Get-BuildSetting,
    Get-VcVarsPath, Find-SdkTool, Invoke-Checked, Write-Step, Get-CargoManifestPath, Get-ProductVersion,
    Set-ProductVersion, Assert-ExeVersion, Build-ReleaseExe, Get-ReleaseAsset
