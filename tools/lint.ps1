# lint.ps1 — the repo's lint gate, the same shape as avi/any-stt's tools/lint.ps1
# (which is itself the C++ equivalent of youtube-time-manager's `pnpm dev:lint`).
#
#   pwsh tools/lint.ps1              check everything, non-zero exit on any finding
#   pwsh tools/lint.ps1 -Fix         apply what can be applied (clang-format -i,
#                                    clang-tidy --fix, dotnet format), then re-check
#   pwsh tools/lint.ps1 -Staged      only C++ files staged for commit (what the
#                                    pre-commit hook runs)
#   pwsh tools/lint.ps1 -InstallHook point git at .githooks so commits are gated
#
# Four passes:
#   clang-format (.clang-format)   C++ layout    <- eslint @stylistic
#   clang-tidy   (.clang-tidy)     correctness   <- oxlint categories.correctness
#   $restrictedSyntax (below)      house rules   <- eslint no-restricted-syntax
#   dotnet format (app\.editorconfig)  C# layout and style, Google C# style
#
# clang-tidy parses with clang-cl so it sees the SAME headers and defines the
# MSVC build uses — the flags are read out of fixedge-split.buildscript, the one
# description of how the native split compiles.
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

# --- which files -----------------------------------------------------------
# The native split's own code and its tests. external\ (MinHook) is vendored and
# stays close to upstream, so linting it would only report other people's opinions.
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
    # --- pass 1: formatting ------------------------------------------------
    Write-Host "[lint] clang-format ($($full.Count) files)"
    if ($Fix) {
        & clang-format -i --style=file $full
    }
    & clang-format --dry-run --Werror --style=file $full 2>&1 | ForEach-Object { Write-Host $_ }
    $formatOk = $LASTEXITCODE -eq 0

    # --- pass 2: correctness -----------------------------------------------
    # Headers are analysed through the translation units that include them, so
    # only .cpp files are handed to clang-tidy (HeaderFilterRegex decides what it
    # reports). A staged header alone still needs its includers analysed, so
    # -Staged analyses every unit whenever any C++ is staged.
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

    # --- pass 3: restricted syntax -----------------------------------------
    # eslint's `no-restricted-syntax`: legal constructs that are not how this
    # codebase is written. One row per rule — what finds it, and the sentence its
    # author needs to read. No fixer on purpose: each is the author's judgement.
    $restrictedSyntax = @(
        @{
            Pattern = 'for\s*\(\s*;\s*;\s*\)'
            Message = 'Use `while (true)` for an infinite loop. `for (;;)` reads as a for loop someone forgot to finish writing.'
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

# --- pass 4: C# formatting and style ---------------------------------------
# app\.editorconfig is the Google C# Style Guide the way .clang-format is the
# Google C++ one, and dotnet format is what reads it. Skipped under -Staged:
# dotnet format works on a project, not a file list, so it would judge unstaged
# files too.
#
# The flag is written out twice rather than splatted in: a conditional array
# unrolls to a bare string, and `@v` on a string drops it — which silently turns
# the check into a FIX (any-stt hit exactly this).
$csharpOk = $true
if (-not $Staged) {
    foreach ($project in @('app\panel\FixEdge.csproj', 'app\host\FixEdge.Host.csproj')) {
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

if ($formatOk -and $tidyOk -and $syntaxOk -and $csharpOk) {
    Write-Host "[lint] clean"
    exit 0
}
Write-Host "[lint] FAILED (format=$formatOk tidy=$tidyOk syntax=$syntaxOk csharp=$csharpOk)"
exit 1
