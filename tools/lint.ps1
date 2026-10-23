<#
.SYNOPSIS
The repo's lint gate over the DLL's C++ and the panel's C#; non-zero exit on any finding.
.DESCRIPTION
Passes: clang-format, clang-tidy, C++ restricted syntax, C# house rules,
Shared.g.cs freshness, dotnet format (app\.editorconfig).
.PARAMETER Fix
Apply clang-format -i, clang-tidy --fix and dotnet format, then re-check.
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
Import-Module (Join-Path $PSScriptRoot 'FixEdge.Build.psm1') -Force
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
    # clang-cl reads the MSVC build's own flags from fixedge-split.buildscript.
    $units = @(Select-Sources (git -C $root ls-files '*.cpp') |
        ForEach-Object { Join-Path $root $_ })
    $vcvars = Get-VcVarsPath

    $nativeDir = Join-Path $root ($nativeRoot -replace '/', '\')
    $standard = Get-BuildSetting 'project:fixedge-split' 'std'
    $defines = (Get-BuildSetting 'project:fixedge-split' 'defines') -split ',' |
        ForEach-Object { '/D' + $_.Trim() }
    $includes = (Get-BuildSetting 'project:fixedge-split' 'includes') -split ',' |
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

# Every Win32 call is a [DllImport] in a Native\ folder, so one anywhere else
# escaped its one home.
$panelRoot = 'app/panel-winui'
$csharpRules = @(
    @{
        Pattern = '\[(DllImport|LibraryImport)\b'
        Allowed = "^$panelRoot/(Panel/)?Native/"
        Message = 'Win32 calls live in a Native/ folder only (the host''s or the panel''s); move this [DllImport] there.'
    }
    @{
        Pattern = 'for\s*\(\s*;\s*;\s*\)'
        Message = 'Use `while (true)` for an infinite loop. `for (;;)` reads as a for loop someone forgot to finish writing.'
    }
)
$csharpFiles = @(git -C $root ls-files "$panelRoot/*.cs")
if ($Staged) {
    $csharpFiles = @(git -C $root diff --cached --name-only --diff-filter=ACMR |
        Where-Object { $_ -match "^$panelRoot/.*\.cs$" })
}
$csharpSyntaxOk = $true
Write-Host "[lint] C# house rules ($($csharpRules.Count) rules, $($csharpFiles.Count) files)"
foreach ($file in $csharpFiles) {
    $lineNumber = 0
    foreach ($line in [System.IO.File]::ReadAllLines((Join-Path $root $file))) {
        $lineNumber++
        $code = $line -replace '//.*$', ''
        foreach ($rule in $csharpRules) {
            $isAllowedHere = $rule.ContainsKey('Allowed') -and ($file -match $rule.Allowed)
            if (($code -match $rule.Pattern) -and -not $isAllowedHere) {
                $csharpSyntaxOk = $false
                Write-Host "${file}:${lineNumber}: $($rule.Message)"
            }
        }
    }
}

# A checked-in Shared.g.cs that disagrees with the @share markers means a C++
# value moved and the commit carries a stale generated file.
$sharedOk = $true
if (-not $Staged) {
    & pwsh -NoProfile -File (Join-Path $PSScriptRoot 'Export-SharedConstants.ps1') -Check
    $sharedOk = $LASTEXITCODE -eq 0
}

# Skipped under -Staged: dotnet format works on a project, not a file list.
#
# The flag is written out twice rather than splatted in: a conditional array
# unrolls to a bare string, and `@v` on a string drops it, which silently turns
# the check into a fix.
$csharpOk = $true
if (-not $Staged) {
    $csharpProjects = @(
        "$panelRoot/EdgePlus.csproj",
        "$panelRoot/Panel/EdgePlus.Panel.csproj",
        "$panelRoot/test/EdgePlus.Tests.csproj"
    )
    foreach ($project in $csharpProjects) {
        $projectPath = Join-Path $root $project
        Write-Host "[lint] dotnet format ($project)"
        foreach ($subcommand in @('whitespace', 'style')) {
            if ($Fix) {
                & dotnet format $subcommand $projectPath
            } else {
                & dotnet format $subcommand $projectPath --verify-no-changes
            }
            if ($LASTEXITCODE -ne 0) {
                $csharpOk = $false
            }
        }
    }
}

if ($formatOk -and $tidyOk -and $syntaxOk -and $csharpSyntaxOk -and $sharedOk -and $csharpOk) {
    Write-Host "[lint] clean"
    exit 0
}
Write-Host "[lint] FAILED (format=$formatOk tidy=$tidyOk syntax=$syntaxOk csharpRules=$csharpSyntaxOk shared=$sharedOk csharp=$csharpOk)"
exit 1
