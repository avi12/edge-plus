<#
.SYNOPSIS
  Generates app\panel-tauri\src-tauri\src\platform\shared.g.rs from the @share
  markers in the native split's C++ sources.

.DESCRIPTION
  Rust cannot include a C++ header, so the values the panel's core and the DLL
  must agree on have the C++ side as their source and the Rust side as a
  generated, committed file.

  `// @share <Name>` exports a value as `shared::<NAME>`: the first string
  literal (L"" ignored) or `= <integer>;` on the marker's line, or, for a
  marker on its own line, the statement under it (strings joined, else its
  integer).

  Run after changing a marked value, and by tools\lint.ps1 with -Check.

.PARAMETER Check
  Compare against the file on disk instead of writing it. Exits 1 when they
  differ.
#>
[CmdletBinding()]
param(
    [switch]$Check
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# What the Rust constant's type is.
enum SharedValueKind {
    String
    Integer
}

# The value found for a marker, already spelled as Rust source.
class SharedValue {
    [SharedValueKind] $Kind
    [string] $RustLiteral

    SharedValue([SharedValueKind] $kind, [string] $rustLiteral) {
        $this.Kind = $kind
        $this.RustLiteral = $rustLiteral
    }
}

class SharedConstant {
    [SharedValue] $Value
    [string] $Origin

    SharedConstant([SharedValue] $value, [string] $origin) {
        $this.Value = $value
        $this.Origin = $origin
    }
}

# One generated file: where it goes, what it must say, and what it came from.
class GeneratedOutput {
    [string] $Path
    [string] $Name
    [string] $Text
    [string] $From

    GeneratedOutput([string] $path, [string] $name, [string] $text, [string] $from) {
        $this.Path = $path
        $this.Name = $name
        $this.Text = $text
        $this.From = $from
    }
}

[string] $repoRoot = Split-Path -Parent $PSScriptRoot
[string] $sourceRoot = Join-Path $repoRoot 'app\native-split\src'
[string] $rustOutputPath = Join-Path $repoRoot 'app\panel-tauri\src-tauri\src\platform\shared.g.rs'
[string] $rustOutputName = 'app/panel-tauri/src-tauri/src/platform/shared.g.rs'

[string] $markerPattern = '//\s*@share\s+(?<name>[A-Za-z_][A-Za-z0-9_]*)\s*$'
[string] $stringPattern = 'L?"(?<value>(?:[^"\\]|\\.)*)"'
[string] $integerPattern = '=\s*(?<value>-?\d+)\s*;'

# Only the escapes these values actually use.
function ConvertFrom-CppEscapes {
    [OutputType([string])]
    param([string] $text)
    return $text -replace '\\\\', "`u{1}" -replace '\\"', '"' -replace "`u{1}", '\'
}

function ConvertTo-StringLiteral {
    [OutputType([string])]
    param([string] $text)
    return '"' + ($text -replace '\\', '\\' -replace '"', '\"') + '"'
}

function Get-RustType {
    [OutputType([string])]
    param([SharedValueKind] $kind)
    if ($kind -eq [SharedValueKind]::String) {
        return '&str'
    }
    return 'u32'
}

function Read-Value {
    [OutputType([SharedValue])]
    param([string] $code)
    [Text.RegularExpressions.Match] $asString = [regex]::Match($code, $stringPattern)
    if ($asString.Success) {
        [string] $text = ''
        foreach ($piece in [regex]::Matches($code, $stringPattern)) {
            $text += ConvertFrom-CppEscapes $piece.Groups['value'].Value
        }
        return [SharedValue]::new([SharedValueKind]::String, (ConvertTo-StringLiteral $text))
    }
    [Text.RegularExpressions.Match] $asInteger = [regex]::Match($code, $integerPattern)
    if ($asInteger.Success) {
        return [SharedValue]::new([SharedValueKind]::Integer, $asInteger.Groups['value'].Value)
    }
    return $null
}

[Collections.Specialized.OrderedDictionary] $constants = [ordered]@{}
[IO.FileInfo[]] $files = @(Get-ChildItem -Path $sourceRoot -Recurse -Include '*.h', '*.cpp' | Sort-Object FullName)
foreach ($file in $files) {
    [string] $relative = [IO.Path]::GetRelativePath($repoRoot, $file.FullName).Replace('\', '/')
    [string[]] $lines = @(Get-Content -LiteralPath $file.FullName)
    [int] $lineNumber = 0
    foreach ($line in $lines) {
        $lineNumber++
        [Text.RegularExpressions.Match] $marker = [regex]::Match($line, $markerPattern)
        if (-not $marker.Success) {
            continue
        }
        [string] $name = $marker.Groups['name'].Value
        if ($constants.Contains($name)) {
            throw "@share $name is claimed twice: $($constants[$name].Origin) and ${relative}:$lineNumber"
        }
        [SharedValue] $value = Read-Value $line.Substring(0, $marker.Index)
        [string] $origin = "${relative}:$lineNumber"
        if (-not $value) {
            # The statement under the marker may span several lines.
            [string] $statement = ''
            for ($index = $lineNumber; $index -lt $lines.Count; $index++) {
                $statement += ' ' + ($lines[$index] -replace '//.*$', '')
                if ($lines[$index] -match ';\s*(//.*)?$') {
                    $origin = "${relative}:$($index + 1)"
                    break
                }
            }
            $value = Read-Value $statement
        }
        if (-not $value) {
            throw "@share $name at ${relative}:$lineNumber has no string or integer value"
        }
        $constants[$name] = [SharedConstant]::new($value, $origin)
    }
}

if ($constants.Count -eq 0) {
    throw 'No @share markers found under app\native-split\src. The generator would empty shared.g.rs.'
}

# SCREAMING_SNAKE_CASE constants; the escaped literal is a Rust string as is.
[Collections.Generic.List[string]] $rustLines = [Collections.Generic.List[string]]::new()
$rustLines.Add('// Generated by tools\Export-SharedConstants.ps1 from the @share markers in the')
$rustLines.Add('// native split''s C++ sources. Do not edit: the next build overwrites it, and')
$rustLines.Add('// tools\lint.ps1 fails if a committed copy disagrees with the headers.')
$rustLines.Add('//')
$rustLines.Add('// Every value here is one the DLL also reads. Change it there.')
foreach ($name in $constants.Keys) {
    [SharedConstant] $constant = $constants[$name]
    [string] $rustName = ($name -creplace '(?<=[a-z0-9])(?=[A-Z])', '_').ToUpperInvariant()
    [string] $rustType = Get-RustType $constant.Value.Kind
    $rustLines.Add('')
    $rustLines.Add("// $($constant.Origin)")
    $rustLines.Add("pub const ${rustName}: $rustType = $($constant.Value.RustLiteral);")
}
[string] $rustGenerated = ($rustLines -join "`n") + "`n"

[GeneratedOutput[]] $outputs = @(
    [GeneratedOutput]::new($rustOutputPath, $rustOutputName, $rustGenerated, "$($constants.Count) @share markers")
)

# Git may check the file out with CRLF line ends; only the text counts.
function Read-Normalized {
    [OutputType([string])]
    param([string] $path)
    if (-not (Test-Path -LiteralPath $path)) {
        return ''
    }
    return [IO.File]::ReadAllText($path) -replace "`r`n", "`n"
}

if ($Check) {
    [bool] $isCurrent = $true
    foreach ($output in $outputs) {
        [string] $current = Read-Normalized $output.Path
        if ($current -ne $output.Text) {
            Write-Host "[shared] $($output.Name) is out of date with the C++ headers." -ForegroundColor Red
            $isCurrent = $false
        } else {
            Write-Host "[shared] $($output.Name) matches $($output.From)"
        }
    }
    if (-not $isCurrent) {
        Write-Host "[shared] run: pwsh tools/Export-SharedConstants.ps1"
        exit 1
    }
    exit 0
}

# Only when it changed, so every build does not recompile the panel.
foreach ($output in $outputs) {
    [string] $current = Read-Normalized $output.Path
    if ($current -ne $output.Text) {
        [IO.File]::WriteAllText($output.Path, $output.Text)
    }
    Write-Host "[shared] $($output.Name) <- $($output.From)"
}
