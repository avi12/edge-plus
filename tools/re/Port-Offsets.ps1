<#
.SYNOPSIS
  Port app/native-split/src/edge/offsets.h from one msedge.dll build to the next.

.DESCRIPTION
  Needs only the two public PDBs (Export-Symbols.ps1 turns each into a
  "rva size name" listing, cached next to the PDB as <pdb>.tsv). For every RVA in the
  `rva` / `addr` namespaces of offsets.h it

    1. finds the symbol(s) at the old RVA, looks each name up in the new PDB
       (method "symbol", or "symbol+rank" when the name is shared by several symbols);
    2. else (a patch site inside a function) finds the containing old function and
       carries the displacement over (method "func+disp");
    3. checks the result: a function's size must match its old size (or is flagged),
       a func+disp site must land on an instruction boundary of the new image, and
       with -OldDll the opening instructions of both functions must have the same
       mnemonics (a "shape" check).

  Anything it cannot place with confidence is "unresolved" and is left unchanged in
  -Apply mode, but the pin is NOT bumped while any entry is unresolved unless
  -AllowUnresolved is given. Hex literals inside offsets.h comments that name an old
  RVA are listed so they can be fixed by hand.

.PARAMETER OldPdb
  The old build's msedge.dll.pdb.
.PARAMETER NewPdb
  The new build's msedge.dll.pdb.
.PARAMETER NewDll
  The new build's msedge.dll.
.PARAMETER OldDll
  The old build's msedge.dll, if it is still there (adds the shape check).
.PARAMETER Table
  Write the old -> new review table (markdown) to this file.
.PARAMETER Apply
  Rewrite offsets.h in place.
.PARAMETER Override
  "NAME=0xNEW:why": a hand-verified RVA for an entry the PDB cannot place (for example
  a patch site in symbol-less cold code).

.EXAMPLE
  pwsh tools\re\Port-Offsets.ps1 -OldPdb old\msedge.dll.pdb -NewPdb new\msedge.dll.pdb -NewDll "...\msedge.dll" -Table docs\re\port-4314.md
#>
param(
    [string] $Offsets = (Join-Path $PSScriptRoot '..\..\app\native-split\src\edge\offsets.h'),
    [Parameter(Mandatory)]
    [string] $OldPdb,
    [Parameter(Mandatory)]
    [string] $NewPdb,
    [Parameter(Mandatory)]
    [string] $NewDll,
    [string] $OldDll,
    [string] $Table,
    [switch] $Apply,
    [switch] $AllowUnresolved,
    [string[]] $Override = @()
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Re.psm1') -Force
Initialize-Dbghelp

[uint64] $shapeBytes = 96
[int] $shapeLength = 12
[string] $scalarPattern = '(?s)(inline constexpr uintptr_t\s+)(\w+)(\s*(?:\[\])?\s*=\s*)((?:kImageBase\s*\+\s*)?)(0x[0-9a-fA-F]+)(;)'
[string] $arrayPattern = '(?s)(inline constexpr uintptr_t\s+)(\w+)\[\]\s*=\s*\{([^}]*)\};'
[string] $hexPattern = '0x[0-9a-fA-F]+'

class Entry {
    [string] $Name
    [uint64] $Value
}

class Vote {
    [uint64] $Candidate
    [string] $Kind
    [bool] $IsSameSize
    [int] $Count
}

class Placed {
    [Nullable[uint64]] $Rva
    [string] $Method
    [string] $Confidence
    [string] $Note

    Placed([Nullable[uint64]] $rva, [string] $method, [string] $confidence, [string] $note) {
        $this.Rva = $rva
        $this.Method = $method
        $this.Confidence = $confidence
        $this.Note = $note
    }
}

class Row {
    [string] $Name
    [uint64] $OldRva
    [Placed] $Placed
    [string] $Label
}

function Import-Symbols {
    [OutputType([EdgePlus.Re.SymbolTable])]
    param([string] $Pdb)
    [string] $tsv = "$Pdb.tsv"
    if (-not (Test-Path $tsv)) {
        & (Join-Path $PSScriptRoot 'Export-Symbols.ps1') -Pdb $Pdb -Out $tsv
    }
    return [EdgePlus.Re.SymbolTable]::new($tsv)
}

function Limit-Text {
    [OutputType([string])]
    param([string] $Text, [int] $Length)
    return $Text.Substring(0, [Math]::Min($Length, $Text.Length))
}

function Get-InstructionStarts {
    [OutputType([uint64[]])]
    param([byte[]] $Image, [uint64] $Start, [int] $Length)
    return [uint64[]]@(Get-Instructions $Image $Start $Length | ForEach-Object { $_.Rva })
}

function Get-Shape {
    [OutputType([string])]
    param([byte[]] $Image, [uint64] $Start)
    [string[]] $mnemonics = @(Get-Instructions $Image $Start $shapeBytes | Select-Object -First $shapeLength | ForEach-Object { $_.Mnemonic })
    return $mnemonics -join ' '
}

function Resolve-Rva {
    [OutputType([Placed])]
    param(
    [uint64] $OldRva,
    [EdgePlus.Re.SymbolTable] $Old,
    [EdgePlus.Re.SymbolTable] $New,
    [byte[]] $NewImage,
    [byte[]] $OldImage
    )
    $names = $Old.StartingAt($OldRva)
    [Collections.Generic.List[Vote]] $votes = @()
    [Collections.Generic.List[string]] $notes = @()
    foreach ($entry in $names) {
        [uint64] $size = $entry.Size
        [string] $label = Limit-Text $entry.Name 60
        $oldGroup = $Old.WithName($entry.Name)
        $newGroup = $New.WithName($entry.Name)
        if ($newGroup.Count -eq 0) {
            $notes.Add("${label}: gone")
            continue
        }
        $sameSizeOld = @($oldGroup | Where-Object { $_.Size -eq $size })
        $sameSizeNew = @($newGroup | Where-Object { $_.Size -eq $size })
        [Vote] $vote = $null
        if ($oldGroup.Count -eq 1 -and $newGroup.Count -eq 1) {
            $vote = [Vote]@{ Candidate = $newGroup[0].Rva; Kind = 'symbol'; IsSameSize = ($size -eq $newGroup[0].Size) }
        } elseif ($sameSizeOld.Count -eq 1 -and $sameSizeNew.Count -eq 1 -and $size -ne 0) {
            # An overloaded or folded name: its size picks the same function.
            $vote = [Vote]@{ Candidate = $sameSizeNew[0].Rva; Kind = 'symbol+size'; IsSameSize = $true }
        } elseif ($oldGroup.Count -eq $newGroup.Count) {
            [int] $rank = 0
            for ([int] $i = 0; $i -lt $oldGroup.Count; $i++) {
                if ($oldGroup[$i].Rva -eq $OldRva -and $oldGroup[$i].Size -eq $size) {
                    $rank = $i
                }
            }
            $vote = [Vote]@{ Candidate = $newGroup[$rank].Rva; Kind = 'symbol+rank'; IsSameSize = ($size -eq $newGroup[$rank].Size) }
        } else {
            $notes.Add("${label}: $($oldGroup.Count) before, $($newGroup.Count) now")
            continue
        }
        [Vote] $known = $votes | Where-Object { $_.Candidate -eq $vote.Candidate -and $_.Kind -eq $vote.Kind -and $_.IsSameSize -eq $vote.IsSameSize } | Select-Object -First 1
        if ($known) {
            $known.Count++
        } else {
            $vote.Count = 1
            $votes.Add($vote)
        }
    }
    if ($votes.Count -gt 0) {
        [Vote] $best = $votes | Sort-Object -Property Count -Descending -Stable | Select-Object -First 1
        [uint64[]] $candidates = @($votes | ForEach-Object { $_.Candidate } | Sort-Object -Unique)
        if ($candidates.Count -ne 1) {
            return [Placed]::new($null, $best.Kind, 'none', 'names disagree: ' + (($candidates | ForEach-Object { '0x{0:x}' -f $_ }) -join ', '))
        }
        [string] $confidence = 'low'
        if ($best.Kind -in 'symbol', 'symbol+size' -and $best.IsSameSize) {
            $confidence = 'high'
        } elseif ($best.Kind -eq 'symbol') {
            $confidence = 'medium'
            $notes.Add('size changed')
        } elseif ($best.IsSameSize) {
            $confidence = 'medium'
        }
        [uint64] $oldExtent = ($names | ForEach-Object { $_.Size } | Measure-Object -Maximum).Maximum
        [uint64] $newExtent = $New.Extent($best.Candidate)
        if ($oldExtent -ne 0 -and $newExtent -ne 0 -and $oldExtent -ne $newExtent) {
            # The name still exists but the body grew or shrank: the code a hook or a
            # struct offset was reversed from has changed. Re-verify by hand.
            $confidence = 'low'
            $notes.Add("size $oldExtent->$newExtent")
        }
        [bool] $isShapeChecked = $OldImage -and $NewImage -and $names[0].Size -ne 0
        if ($isShapeChecked -and (Get-Shape $OldImage $OldRva) -ne (Get-Shape $NewImage $best.Candidate)) {
            $confidence = 'low'
            $notes.Add('opening instructions differ')
        }
        return [Placed]::new($best.Candidate, $best.Kind, $confidence, ($notes -join '; '))
    }
    if ($names.Count -eq 0) {
        [EdgePlus.Re.Owner] $owner = $Old.OwnerOf($OldRva)
        if ($owner) {
            [uint64] $displacement = $OldRva - $owner.Start
            [string] $ownerLabel = Limit-Text $Old.StartingAt($owner.Start)[0].Name 50
            [Placed] $placed = Resolve-Rva $owner.Start $Old $New $NewImage $OldImage
            if ($null -eq $placed.Rva) {
                return [Placed]::new($null, 'func+disp', 'none', "owner ${ownerLabel}: $($placed.Note)")
            }
            [string] $confidence = $placed.Confidence
            [string] $note = $placed.Note
            [uint64] $newSize = $New.Extent($placed.Rva)
            [uint64] $oldSize = $Old.Extent($owner.Start)
            if ($oldSize -ne $newSize) {
                $confidence = 'low'
                $note += "; owner size $oldSize->$newSize"
            } elseif ($confidence -eq 'high') {
                $confidence = 'medium'
            }
            if ($NewImage) {
                [uint64[]] $starts = Get-InstructionStarts $NewImage $placed.Rva ([int]$displacement + 16)
                if ($starts -notcontains ($placed.Rva + $displacement)) {
                    # A patched immediate or displacement byte: legitimate, but only the
                    # DLL's own byte check can vouch for it.
                    $note += '; inside an instruction (patched operand)'
                    $confidence = 'low'
                }
            }
            return [Placed]::new($placed.Rva + $displacement, 'func+disp', $confidence, ('in {0} +0x{1:x} {2}' -f $ownerLabel, $displacement, $note))
        }
    }
    [string] $why = if ($notes.Count) { $notes -join '; ' } else { 'no symbol' }
    return [Placed]::new($null, 'none', 'none', $why)
}

# Every scalar and array entry of the rva and addr namespaces, arrays as name[position].
function Read-Entries {
    [OutputType([Entry])]
    param([string] $Section)
    foreach ($match in [regex]::Matches($Section, $arrayPattern)) {
        [int] $position = 0
        foreach ($hex in [regex]::Matches($match.Groups[3].Value, $hexPattern)) {
            [Entry]@{ Name = "$($match.Groups[2].Value)[$position]"; Value = [Convert]::ToUInt64($hex.Value.Substring(2), 16) }
            $position++
        }
    }
    foreach ($match in [regex]::Matches($Section, $scalarPattern)) {
        [Entry]@{ Name = $match.Groups[2].Value; Value = [Convert]::ToUInt64($match.Groups[5].Value.Substring(2), 16) }
    }
}

# --- main ----------------------------------------------------------------------

[string] $offsetsPath = (Resolve-Path $Offsets).Path
[string] $text = [IO.File]::ReadAllText($offsetsPath)
[int] $sectionStart = $text.IndexOf('namespace rva {')
[int] $sectionEnd = $text.IndexOf('namespace off {')
[string] $section = $text.Substring($sectionStart, $sectionEnd - $sectionStart)

[EdgePlus.Re.SymbolTable] $old = Import-Symbols $OldPdb
[EdgePlus.Re.SymbolTable] $new = Import-Symbols $NewPdb
[byte[]] $newImage = Read-MappedImage $NewDll
[byte[]] $oldImage = if ($OldDll -and (Test-Path $OldDll)) { Read-MappedImage $OldDll } else { $null }

[Collections.Generic.Dictionary[string, Placed]] $overrides = @{}
foreach ($spec in $Override) {
    [string[]] $assignment = $spec -split '=', 2
    [string[]] $parts = $assignment[1] -split ':', 2
    $overrides[$assignment[0]] = [Placed]::new([Convert]::ToUInt64($parts[0].Substring(2), 16), 'manual', 'verified', $parts[1])
}

[Collections.Generic.List[Row]] $rows = @()
foreach ($entry in Read-Entries $section) {
    [Placed] $placed = if ($overrides.ContainsKey($entry.Name)) { $overrides[$entry.Name] } else { Resolve-Rva $entry.Value $old $new $newImage $oldImage }
    [string] $label = ''
    if ($old.HasSymbolAt($entry.Value)) {
        $label = [EdgePlus.Re.Dbghelp]::Undecorate($old.StartingAt($entry.Value)[0].Name)
    }
    $rows.Add([Row]@{ Name = $entry.Name; OldRva = $entry.Value; Placed = $placed; Label = $label })
}

Write-Host "$($rows.Count) entries"
$rows | Group-Object { '{0,-12} {1,-7}' -f $(if ($null -ne $_.Placed.Rva) { $_.Placed.Method } else { 'unresolved' }), $_.Placed.Confidence } |
    Sort-Object Name | ForEach-Object { '  {0} {1}' -f $_.Name, $_.Count }
[Row[]] $unresolved = @($rows | Where-Object { $null -eq $_.Placed.Rva })
foreach ($row in $unresolved) {
    Write-Host ('  UNRESOLVED {0} 0x{1:x}: {2}' -f $row.Name, $row.OldRva, $row.Placed.Note)
}

if ($Table) {
    [Collections.Generic.List[string]] $lines = @(
        '| entry | old RVA | new RVA | delta | method | confidence | symbol / note |'
        '|---|---|---|---|---|---|---|'
    )
    foreach ($row in $rows) {
        [Placed] $placed = $row.Placed
        [string] $shown = if ($null -ne $placed.Rva) { '0x{0:x}' -f $placed.Rva } else { 'UNRESOLVED' }
        [string] $delta = ''
        if ($null -ne $placed.Rva) {
            [int64] $difference = [int64]$placed.Rva - [int64]$row.OldRva
            $delta = if ($difference -lt 0) { '-0x{0:x}' -f (-$difference) } else { '+0x{0:x}' -f $difference }
        }
        [string] $detail = ((Limit-Text $row.Label 90) + $(if ($placed.Note) { "; $($placed.Note)" } else { '' })).Replace('|', '\|')
        $lines.Add("| $($row.Name) | 0x$('{0:x}' -f $row.OldRva) | $shown | $delta | $($placed.Method) | $($placed.Confidence) | $detail |")
    }
    [IO.File]::WriteAllLines($Table, $lines)
}

if ($Apply) {
    if ($unresolved.Count -gt 0 -and -not $AllowUnresolved) {
        throw 'refusing to apply with unresolved entries (use -AllowUnresolved)'
    }
    [Collections.Generic.Dictionary[uint64, uint64]] $mapping = @{}
    foreach ($row in $rows) {
        if ($null -ne $row.Placed.Rva) {
            $mapping[$row.OldRva] = $row.Placed.Rva
        }
    }
    $section = [regex]::Replace($section, $arrayPattern, {
        param([Text.RegularExpressions.Match] $match)
        [string] $inner = [regex]::Replace($match.Groups[3].Value, $hexPattern, {
            param([Text.RegularExpressions.Match] $hex)
            [uint64] $value = [Convert]::ToUInt64($hex.Value.Substring(2), 16)
            if ($mapping.ContainsKey($value)) { '0x{0:x}' -f $mapping[$value] } else { $hex.Value }
        })
        $match.Groups[1].Value + $match.Groups[2].Value + '[] = {' + $inner + '};'
    })
    $section = [regex]::Replace($section, $scalarPattern, {
        param([Text.RegularExpressions.Match] $match)
        [uint64] $value = [Convert]::ToUInt64($match.Groups[5].Value.Substring(2), 16)
        if (-not $mapping.ContainsKey($value)) {
            return $match.Value
        }
        [int] $digits = [Math]::Max($match.Groups[5].Value.Length - 2, 0)
        $match.Groups[1].Value + $match.Groups[2].Value + $match.Groups[3].Value + $match.Groups[4].Value + '0x' + $mapping[$value].ToString("x$digits") + $match.Groups[6].Value
    })
    $text = $text.Substring(0, $sectionStart) + $section + $text.Substring($sectionEnd)
    [IO.File]::WriteAllText($offsetsPath, $text, [Text.UTF8Encoding]::new($false))
    Write-Host 'offsets.h rewritten (clang-format it; bump kVersion* and the panel pin by hand)'
}

# Old RVAs quoted in comments are not rewritten; list them for a manual fix.
[int] $lineNumber = 0
foreach ($line in $text -split "`n") {
    $lineNumber++
    if (-not $line.Contains('//')) {
        continue
    }
    [string] $comment = $line.Substring($line.IndexOf('//') + 2)
    foreach ($literal in [regex]::Matches($comment, $hexPattern)) {
        [uint64] $value = [Convert]::ToUInt64($literal.Value.Substring(2), 16)
        if ($value -gt 0x10000 -and $old.HasSymbolAt($value)) {
            [string] $name = [EdgePlus.Re.Dbghelp]::Undecorate($old.StartingAt($value)[0].Name)
            Write-Host ('  comment line {0}: {1} is an old RVA ({2})' -f $lineNumber, $literal.Value, (Limit-Text $name 60))
        }
    }
}
