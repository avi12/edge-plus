<#
.SYNOPSIS
The repo's lint gate over the DLL's C++ and the panel's Rust and TypeScript; non-zero exit on any finding.
.DESCRIPTION
Passes: clang-format, clang-tidy, C++ restricted syntax, shared.g.rs
freshness, the panel's own gate (oxlint, eslint, clippy, rustfmt, tsc,
fallow), ui/protocol.g.ts freshness and the freshness of the components
svelte-winui generates from WinUI's metadata.
.PARAMETER Fix
Apply clang-format -i, clang-tidy --fix and the panel's fixers, then re-check.
.PARAMETER Staged
Only files staged for commit (what the pre-commit hook runs).
.PARAMETER InstallHook
Point git at .githooks so commits are gated.
#>
[CmdletBinding()]
param(
    [switch]$Fix,
    [switch]$Staged,
    [switch]$InstallHook
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Build.psm1') -Force
$root = Get-RepoRoot

if ($InstallHook) {
    git -C $root config core.hooksPath .githooks
    Write-Host "[lint] pre-commit hook enabled (git config core.hooksPath .githooks)"
    exit 0
}

# external\ (MinHook) is vendored and stays close to upstream, so it is not linted.
$nativeRoot = 'app/native-split'
function Select-Sources {
    param([string[]]$Paths)
    $Paths | Where-Object { $_ -match "^$nativeRoot/(src|test)/.*\.(cpp|h)$" }
}

if ($Staged) {
    $files = Select-Sources (git -C $root diff --cached --name-only --diff-filter=ACMR)
} else {
    $files = Select-Sources (git -C $root ls-files '*.cpp' '*.h')
}
$full = @($files | ForEach-Object { Join-Path $root $_ })
$hasCppInScope = $full.Count -gt 0

$formatOk = $true
$tidyOk = $true
$syntaxOk = $true
if (-not $hasCppInScope) {
    Write-Host "[lint] no C++ in scope"
}

if ($hasCppInScope) {
    Write-Host "[lint] clang-format ($($full.Count) files)"
    if ($Fix) {
        & clang-format -i --style=file $full
    }
    & clang-format --dry-run --Werror --style=file $full 2>&1 | ForEach-Object { Write-Host $_ }
    $formatOk = $LASTEXITCODE -eq 0

    # Headers are analysed through the units that include them (HeaderFilterRegex
    # decides what is reported), so a staged header alone still needs every unit.
    # clang-cl reads the MSVC build's own flags from edgeplus-split.buildscript.
    $units = @(Select-Sources (git -C $root ls-files '*.cpp') |
        ForEach-Object { Join-Path $root $_ })
    $vcvars = Get-VcVarsPath

    $nativeDir = Join-Path $root ($nativeRoot -replace '/', '\')
    $standard = Get-BuildSetting 'project:edgeplus-split' 'std'
    $defines = (Get-BuildSetting 'project:edgeplus-split' 'defines') -split ',' |
        ForEach-Object { '/D' + $_.Trim() }
    $includes = (Get-BuildSetting 'project:edgeplus-split' 'includes') -split ',' |
        ForEach-Object { '/I"' + (Join-Path $nativeDir ($_.Trim() -replace '/', '\')) + '"' }
    $clangArgs = "--driver-mode=cl /EHsc /std:c++$standard " + (($defines + $includes) -join ' ')
    $tidyFlags = if ($Fix) { '--fix --fix-errors' } else { '' }

    Write-Host "[lint] clang-tidy ($($units.Count) translation units)"
    foreach ($unit in $units) {
        $cmd = "call `"$vcvars`" >nul 2>&1 && clang-tidy --quiet $tidyFlags `"$unit`" -- $clangArgs"
        $out = cmd /c $cmd 2>&1
        if ($LASTEXITCODE -ne 0) {
            $tidyOk = $false
            $out | ForEach-Object { Write-Host $_ }
        }
    }

    # Legal constructs that are not how this codebase is written. No fixer on
    # purpose: each is the author's judgement.
    $restrictedSyntax = @(
        @{
            Pattern = 'for\s*\(\s*;\s*;\s*\)'
            Message = 'Use `while (true)` for an infinite loop. `for (;;)` reads as a for loop someone forgot to finish writing.'
        }
        @{
            # A g_ global or an o_ hook original, declared without constinit.
            Pattern = '^inline\s+(?!constexpr\b|constinit\b)[\w:<>, ]+[*&]?\s+[go]_\w+\s*(\[[^\]]*\])?\s*(=|\{|;)'
            Message = 'Declare a mutable global `inline constinit`: a dynamic initializer would run in DllMain under the loader lock, in an order the linker picks.'
        }
    )

    Write-Host "[lint] restricted syntax ($($restrictedSyntax.Count) rules)"
    foreach ($path in $full) {
        $lineNumber = 0
        foreach ($line in [System.IO.File]::ReadAllLines($path)) {
            $lineNumber++
            # A rule names a construct, not a mention of one, so comments go first.
            $code = $line -replace '//.*$', ''
            foreach ($rule in $restrictedSyntax) {
                if ($code -match $rule.Pattern) {
                    $syntaxOk = $false
                    Write-Host "${path}:${lineNumber}: $($rule.Message)"
                }
            }
        }
    }
}

# A checked-in shared.g.rs that disagrees with the @share markers means a C++
# value moved and the commit carries a stale generated file.
$sharedOk = $true
if (-not $Staged) {
    & pwsh -NoProfile -File (Join-Path $PSScriptRoot 'Export-SharedConstants.ps1') -Check
    $sharedOk = $LASTEXITCODE -eq 0
}

# The panel: oxlint, eslint, clippy at -D warnings, rustfmt, tsc and the
# fallow audit (its package.json), and the window's protocol, which `cargo
# test` writes from protocol.rs, still matching what is committed. Skipped
# under -Staged: these run on the project, not a file list.
$panelOk = $true
if (-not $Staged) {
    $panel = Join-Path $root 'app\panel-tauri'
    $script = if ($Fix) { 'lint' } else { 'lint:verify' }
    Write-Host "[lint] panel (pnpm $script)"
    & pnpm --dir $panel $script
    $panelOk = $LASTEXITCODE -eq 0

    $protocol = Join-Path $panel 'ui\protocol.g.ts'
    $before = (Get-FileHash $protocol).Hash
    Write-Host '[lint] window protocol (cargo test export_the_window_types)'
    & cargo test --manifest-path (Join-Path $panel 'src-tauri\Cargo.toml') --quiet export_the_window_types | Out-Null
    $isProtocolCurrent = $LASTEXITCODE -eq 0 -and (Get-FileHash $protocol).Hash -eq $before
    if (-not $isProtocolCurrent) {
        Write-Host '[lint] ui/protocol.g.ts was out of date with protocol.rs; it is rewritten now, commit it.'
        $panelOk = $false
    }

    # The svelte-winui components are generated from WinUI's metadata; a
    # changed generator or an updated Windows App SDK leaves them stale.
    $components = Join-Path $root 'packages\svelte-winui'
    $componentsScript = if ($Fix) { 'gen:components' } else { 'gen:components:check' }
    Write-Host "[lint] generated components (pnpm $componentsScript)"
    & pnpm --dir $components $componentsScript
    if ($LASTEXITCODE -ne 0) {
        $panelOk = $false
    }
}

if ($formatOk -and $tidyOk -and $syntaxOk -and $sharedOk -and $panelOk) {
    Write-Host "[lint] clean"
    exit 0
}
Write-Host "[lint] FAILED (format=$formatOk tidy=$tidyOk syntax=$syntaxOk shared=$sharedOk panel=$panelOk)"
exit 1
