<#
.SYNOPSIS
  Port app/dll/src/edge/offsets.h from one msedge.dll build to the next.

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
    [string] $Offsets = (Join-Path $PSScriptRoot '..\..\app\dll\src\edge\offsets.h'),
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
[int] $symbolLabelLimit = 60
[int] $ownerLabelLimit = 50
[int] $tableLabelLimit = 90
[int] $instructionSlack = 16
[uint64] $minimumCommentRva = 0x10000
[string] $scalarPattern = '(?s)(inline constexpr uintptr_t\s+)(\w+)(\s*(?:\[\])?\s*=\s*)((?:kImageBase\s*\+\s*)?)(0x[0-9a-fA-F]+)(;)'
[string] $arrayPattern = '(?s)(inline constexpr uintptr_t\s+)(\w+)\[\]\s*=\s*\{([^}]*)\};'
[string] $hexPattern = '0x[0-9a-fA-F]+'
[string] $rvaSectionMarker = 'namespace rva {'
[string] $rvaSectionEndMarker = 'namespace off {'

[string] $methodSymbol = 'symbol'
[string] $methodSymbolSize = 'symbol+size'
[string] $methodSymbolRank = 'symbol+rank'
[string] $methodFunctionDisplacement = 'func+disp'
[string] $methodManual = 'manual'
[string] $methodNone = 'none'
[string] $methodUnresolved = 'unresolved'
[string] $confidenceHigh = 'high'
[string] $confidenceMedium = 'medium'
[string] $confidenceLow = 'low'
[string] $confidenceNone = 'none'
[string] $confidenceVerified = 'verified'

class Entry {
    [string] $Name
    [uint64] $Value
}

class Vote {
    [uint64] $Candidate
    [string] $Kind
    [bool] $IsSameSize
    [int] $Count

    Vote([uint64] $candidate, [string] $kind, [bool] $isSameSize) {
        $this.Candidate = $candidate
        $this.Kind = $kind
        $this.IsSameSize = $isSameSize
    }
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

    [bool] IsResolved() {
        return $null -ne $this.Rva
    }
}

class Row {
    [string] $Name
    [uint64] $OldRva
    [Placed] $Placed
    [string] $Label
}

class OffsetsSource {
    [string] $Path
    [string] $Text
    [int] $SectionStart
    [int] $SectionEnd
    [string] $Section

    OffsetsSource([string] $path, [string] $text, [int] $sectionStart, [int] $sectionEnd) {
        $this.Path = $path
        $this.Text = $text
        $this.SectionStart = $sectionStart
        $this.SectionEnd = $sectionEnd
        $this.Section = $text.Substring($sectionStart, $sectionEnd - $sectionStart)
    }
}

function ConvertFrom-HexLiteral {
    [OutputType([uint64])]
    param([string] $Literal)
    return [Convert]::ToUInt64($Literal.Substring(2), 16)
}

function Format-Rva {
    [OutputType([string])]
    param([uint64] $Rva)
    return '0x{0:x}' -f $Rva
}

function Limit-Text {
    [OutputType([string])]
    param([string] $Text, [int] $Length)
    return $Text.Substring(0, [Math]::Min($Length, $Text.Length))
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

function Get-UndecoratedName {
    [OutputType([string])]
    param([EdgePlus.Re.SymbolTable] $Symbols, [uint64] $Rva)
    return [EdgePlus.Re.Dbghelp]::Undecorate($Symbols.StartingAt($Rva)[0].Name)
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

function Get-RankInGroup {
    [OutputType([int])]
    param($Group, [uint64] $Rva, [uint64] $Size)
    [int] $rank = 0
    for ([int] $i = 0; $i -lt $Group.Count; $i++) {
        [bool] $isSameSymbol = $Group[$i].Rva -eq $Rva -and $Group[$i].Size -eq $Size
        if ($isSameSymbol) {
            $rank = $i
        }
    }
    return $rank
}

# One name's vote for where the old RVA moved to, or $null (with the reason in $Notes).
function Get-SymbolVote {
    [OutputType([Vote])]
    param(
        [EdgePlus.Re.NamedSize] $Symbol,
        [uint64] $OldRva,
        [EdgePlus.Re.SymbolTable] $Old,
        [EdgePlus.Re.SymbolTable] $New,
        [Collections.Generic.List[string]] $Notes
    )
    [uint64] $size = $Symbol.Size
    [string] $label = Limit-Text $Symbol.Name $symbolLabelLimit
    $oldGroup = $Old.WithName($Symbol.Name)
    $newGroup = $New.WithName($Symbol.Name)
    if ($newGroup.Count -eq 0) {
        $Notes.Add("${label}: gone")
        return $null
    }
    $isUnique = $oldGroup.Count -eq 1 -and $newGroup.Count -eq 1
    if ($isUnique) {
        return [Vote]::new($newGroup[0].Rva, $methodSymbol, ($size -eq $newGroup[0].Size))
    }
    $sameSizeOld = @($oldGroup | Where-Object { $_.Size -eq $size })
    $sameSizeNew = @($newGroup | Where-Object { $_.Size -eq $size })
    # An overloaded or folded name: its size picks the same function.
    $isPickedBySize = $sameSizeOld.Count -eq 1 -and $sameSizeNew.Count -eq 1 -and $size -ne 0
    if ($isPickedBySize) {
        return [Vote]::new($sameSizeNew[0].Rva, $methodSymbolSize, $true)
    }
    if ($oldGroup.Count -ne $newGroup.Count) {
        $Notes.Add("${label}: $($oldGroup.Count) before, $($newGroup.Count) now")
        return $null
    }
    [int] $rank = Get-RankInGroup $oldGroup $OldRva $size
    return [Vote]::new($newGroup[$rank].Rva, $methodSymbolRank, ($size -eq $newGroup[$rank].Size))
}

function Add-Vote {
    param([Collections.Generic.List[Vote]] $Votes, [Vote] $Vote)
    [Vote] $known = $Votes | Where-Object { $_.Candidate -eq $Vote.Candidate -and $_.Kind -eq $Vote.Kind -and $_.IsSameSize -eq $Vote.IsSameSize } | Select-Object -First 1
    if ($known) {
        $known.Count++
        return
    }
    $Vote.Count = 1
    $Votes.Add($Vote)
}

function Get-SymbolVotes {
    [OutputType([Vote[]])]
    param(
        $Names,
        [uint64] $OldRva,
        [EdgePlus.Re.SymbolTable] $Old,
        [EdgePlus.Re.SymbolTable] $New,
        [Collections.Generic.List[string]] $Notes
    )
    [Collections.Generic.List[Vote]] $votes = @()
    foreach ($symbol in $Names) {
        [Vote] $vote = Get-SymbolVote $symbol $OldRva $Old $New $Notes
        if ($vote) {
            Add-Vote $votes $vote
        }
    }
    return $votes.ToArray()
}

function Get-VoteConfidence {
    [OutputType([string])]
    param([Vote] $Best, [Collections.Generic.List[string]] $Notes)
    $isExactSymbol = $Best.Kind -in $methodSymbol, $methodSymbolSize
    if ($isExactSymbol -and $Best.IsSameSize) {
        return $confidenceHigh
    }
    if ($Best.Kind -eq $methodSymbol) {
        $Notes.Add('size changed')
        return $confidenceMedium
    }
    if ($Best.IsSameSize) {
        return $confidenceMedium
    }
    return $confidenceLow
}

# The name still exists but the body grew or shrank, or its opening instructions
# changed: the code a hook or a struct offset was reversed from has changed.
function Resolve-BySymbol {
    [OutputType([Placed])]
    param(
        [Vote[]] $Votes,
        $Names,
        [uint64] $OldRva,
        [EdgePlus.Re.SymbolTable] $New,
        [byte[]] $NewImage,
        [byte[]] $OldImage,
        [Collections.Generic.List[string]] $Notes
    )
    [Vote] $best = $Votes | Sort-Object -Property Count -Descending -Stable | Select-Object -First 1
    [uint64[]] $candidates = @($Votes | ForEach-Object { $_.Candidate } | Sort-Object -Unique)
    if ($candidates.Count -ne 1) {
        return [Placed]::new($null, $best.Kind, $confidenceNone, 'names disagree: ' + (($candidates | ForEach-Object { Format-Rva $_ }) -join ', '))
    }
    [string] $confidence = Get-VoteConfidence $best $Notes
    [uint64] $oldExtent = ($Names | ForEach-Object { $_.Size } | Measure-Object -Maximum).Maximum
    [uint64] $newExtent = $New.Extent($best.Candidate)
    $hasChangedExtent = $oldExtent -ne 0 -and $newExtent -ne 0 -and $oldExtent -ne $newExtent
    if ($hasChangedExtent) {
        $confidence = $confidenceLow
        $Notes.Add("size $oldExtent->$newExtent")
    }
    [bool] $canCompareShape = $OldImage -and $NewImage -and $Names[0].Size -ne 0
    $hasChangedShape = $canCompareShape -and (Get-Shape $OldImage $OldRva) -ne (Get-Shape $NewImage $best.Candidate)
    if ($hasChangedShape) {
        $confidence = $confidenceLow
        $Notes.Add('opening instructions differ')
    }
    return [Placed]::new($best.Candidate, $best.Kind, $confidence, ($Notes -join '; '))
}

function Test-LandsOnInstruction {
    [OutputType([bool])]
    param([byte[]] $Image, [uint64] $Start, [uint64] $Displacement)
    [uint64[]] $starts = Get-InstructionStarts $Image $Start ([int]$Displacement + $instructionSlack)
    return $starts -contains ($Start + $Displacement)
}

# A patch site inside a function: place the owning function, then carry the displacement over.
function Resolve-InsideOwner {
    [OutputType([Placed])]
    param(
        [uint64] $OldRva,
        [EdgePlus.Re.Owner] $Owner,
        [EdgePlus.Re.SymbolTable] $Old,
        [EdgePlus.Re.SymbolTable] $New,
        [byte[]] $NewImage,
        [byte[]] $OldImage
    )
    [uint64] $displacement = $OldRva - $Owner.Start
    [string] $ownerLabel = Limit-Text $Old.StartingAt($Owner.Start)[0].Name $ownerLabelLimit
    [Placed] $placed = Resolve-Rva $Owner.Start $Old $New $NewImage $OldImage
    if (-not $placed.IsResolved()) {
        return [Placed]::new($null, $methodFunctionDisplacement, $confidenceNone, "owner ${ownerLabel}: $($placed.Note)")
    }
    [string] $confidence = $placed.Confidence
    [string] $note = $placed.Note
    [uint64] $newSize = $New.Extent($placed.Rva)
    [uint64] $oldSize = $Old.Extent($Owner.Start)
    if ($oldSize -ne $newSize) {
        $confidence = $confidenceLow
        $note += "; owner size $oldSize->$newSize"
    } elseif ($confidence -eq $confidenceHigh) {
        $confidence = $confidenceMedium
    }
    # A patched immediate or displacement byte is legitimate, but only the DLL's own
    # byte check can vouch for it.
    $isPatchedOperand = $NewImage -and -not (Test-LandsOnInstruction $NewImage $placed.Rva $displacement)
    if ($isPatchedOperand) {
        $note += '; inside an instruction (patched operand)'
        $confidence = $confidenceLow
    }
    return [Placed]::new($placed.Rva + $displacement, $methodFunctionDisplacement, $confidence, ('in {0} +0x{1:x} {2}' -f $ownerLabel, $displacement, $note))
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
    [Collections.Generic.List[string]] $notes = @()
    [Vote[]] $votes = @(Get-SymbolVotes $names $OldRva $Old $New $notes)
    if ($votes.Count -gt 0) {
        return Resolve-BySymbol $votes $names $OldRva $New $NewImage $OldImage $notes
    }
    if ($names.Count -eq 0) {
        [EdgePlus.Re.Owner] $owner = $Old.OwnerOf($OldRva)
        if ($owner) {
            return Resolve-InsideOwner $OldRva $owner $Old $New $NewImage $OldImage
        }
    }
    [string] $why = if ($notes.Count) { $notes -join '; ' } else { 'no symbol' }
    return [Placed]::new($null, $methodNone, $confidenceNone, $why)
}

function Read-OffsetsSource {
    [OutputType([OffsetsSource])]
    param([string] $Path)
    [string] $resolvedPath = (Resolve-Path $Path).Path
    [string] $text = [IO.File]::ReadAllText($resolvedPath)
    return [OffsetsSource]::new($resolvedPath, $text, $text.IndexOf($rvaSectionMarker), $text.IndexOf($rvaSectionEndMarker))
}

# Every scalar and array entry of the rva and addr namespaces, arrays as name[position].
function Read-Entries {
    [OutputType([Entry])]
    param([string] $Section)
    foreach ($match in [regex]::Matches($Section, $arrayPattern)) {
        [int] $position = 0
        foreach ($hex in [regex]::Matches($match.Groups[3].Value, $hexPattern)) {
            [Entry]@{ Name = "$($match.Groups[2].Value)[$position]"; Value = ConvertFrom-HexLiteral $hex.Value }
            $position++
        }
    }
    foreach ($match in [regex]::Matches($Section, $scalarPattern)) {
        [Entry]@{ Name = $match.Groups[2].Value; Value = ConvertFrom-HexLiteral $match.Groups[5].Value }
    }
}

function Read-Overrides {
    [OutputType([Collections.Generic.Dictionary[string, Placed]])]
    param([string[]] $Specs)
    [Collections.Generic.Dictionary[string, Placed]] $overrides = @{}
    foreach ($spec in $Specs) {
        [string[]] $assignment = $spec -split '=', 2
        [string[]] $parts = $assignment[1] -split ':', 2
        $overrides[$assignment[0]] = [Placed]::new((ConvertFrom-HexLiteral $parts[0]), $methodManual, $confidenceVerified, $parts[1])
    }
    return $overrides
}

function New-Rows {
    [OutputType([Row[]])]
    param(
        [Entry[]] $Entries,
        [Collections.Generic.Dictionary[string, Placed]] $Overrides,
        [EdgePlus.Re.SymbolTable] $Old,
        [EdgePlus.Re.SymbolTable] $New,
        [byte[]] $NewImage,
        [byte[]] $OldImage
    )
    [Collections.Generic.List[Row]] $rows = @()
    foreach ($entry in $Entries) {
        [Placed] $placed = if ($Overrides.ContainsKey($entry.Name)) { $Overrides[$entry.Name] } else { Resolve-Rva $entry.Value $Old $New $NewImage $OldImage }
        [string] $label = ''
        if ($Old.HasSymbolAt($entry.Value)) {
            $label = Get-UndecoratedName $Old $entry.Value
        }
        $rows.Add([Row]@{ Name = $entry.Name; OldRva = $entry.Value; Placed = $placed; Label = $label })
    }
    return $rows.ToArray()
}

function Get-SummaryGroup {
    [OutputType([string])]
    param([Row] $Row)
    [string] $method = if ($Row.Placed.IsResolved()) { $Row.Placed.Method } else { $methodUnresolved }
    return '{0,-12} {1,-7}' -f $method, $Row.Placed.Confidence
}

function Write-Summary {
    param([Row[]] $Rows)
    Write-Host "$($Rows.Count) entries"
    $Rows | Group-Object { Get-SummaryGroup $_ } | Sort-Object Name | ForEach-Object { '  {0} {1}' -f $_.Name, $_.Count }
    foreach ($row in $Rows) {
        if (-not $row.Placed.IsResolved()) {
            Write-Host ('  UNRESOLVED {0} {1}: {2}' -f $row.Name, (Format-Rva $row.OldRva), $row.Placed.Note)
        }
    }
}

function Format-Delta {
    [OutputType([string])]
    param([Row] $Row)
    if (-not $Row.Placed.IsResolved()) {
        return ''
    }
    [int64] $difference = [int64]$Row.Placed.Rva - [int64]$Row.OldRva
    if ($difference -lt 0) {
        return '-' + (Format-Rva (-$difference))
    }
    return '+' + (Format-Rva $difference)
}

function Format-TableLine {
    [OutputType([string])]
    param([Row] $Row)
    [Placed] $placed = $Row.Placed
    [string] $shown = if ($placed.IsResolved()) { Format-Rva $placed.Rva } else { 'UNRESOLVED' }
    [string] $note = if ($placed.Note) { "; $($placed.Note)" } else { '' }
    [string] $detail = ((Limit-Text $Row.Label $tableLabelLimit) + $note).Replace('|', '\|')
    return "| $($Row.Name) | $(Format-Rva $Row.OldRva) | $shown | $(Format-Delta $Row) | $($placed.Method) | $($placed.Confidence) | $detail |"
}

function Write-Table {
    param([Row[]] $Rows, [string] $Path)
    [Collections.Generic.List[string]] $lines = @(
        '| entry | old RVA | new RVA | delta | method | confidence | symbol / note |'
        '|---|---|---|---|---|---|---|'
    )
    foreach ($row in $Rows) {
        $lines.Add((Format-TableLine $row))
    }
    [IO.File]::WriteAllLines($Path, $lines)
}

function Get-RvaMapping {
    [OutputType([Collections.Generic.Dictionary[uint64, uint64]])]
    param([Row[]] $Rows)
    [Collections.Generic.Dictionary[uint64, uint64]] $mapping = @{}
    foreach ($row in $Rows) {
        if ($row.Placed.IsResolved()) {
            $mapping[$row.OldRva] = $row.Placed.Rva
        }
    }
    return $mapping
}

function Convert-ArrayMatch {
    [OutputType([string])]
    param([Text.RegularExpressions.Match] $Match, [Collections.Generic.Dictionary[uint64, uint64]] $Mapping)
    [string] $inner = [regex]::Replace($Match.Groups[3].Value, $hexPattern, {
        param([Text.RegularExpressions.Match] $hex)
        [uint64] $value = ConvertFrom-HexLiteral $hex.Value
        if ($Mapping.ContainsKey($value)) {
            return Format-Rva $Mapping[$value]
        }
        return $hex.Value
    })
    return $Match.Groups[1].Value + $Match.Groups[2].Value + '[] = {' + $inner + '};'
}

function Convert-ScalarMatch {
    [OutputType([string])]
    param([Text.RegularExpressions.Match] $Match, [Collections.Generic.Dictionary[uint64, uint64]] $Mapping)
    [uint64] $value = ConvertFrom-HexLiteral $Match.Groups[5].Value
    if (-not $Mapping.ContainsKey($value)) {
        return $Match.Value
    }
    [int] $digits = [Math]::Max($Match.Groups[5].Value.Length - 2, 0)
    return $Match.Groups[1].Value + $Match.Groups[2].Value + $Match.Groups[3].Value + $Match.Groups[4].Value + '0x' + $Mapping[$value].ToString("x$digits") + $Match.Groups[6].Value
}

# The whole file's text with every placed RVA rewritten in the rva and addr namespaces.
function Get-RewrittenText {
    [OutputType([string])]
    param([OffsetsSource] $Source, [Row[]] $Rows)
    [Collections.Generic.Dictionary[uint64, uint64]] $mapping = Get-RvaMapping $Rows
    [string] $section = [regex]::Replace($Source.Section, $arrayPattern, {
        param([Text.RegularExpressions.Match] $match)
        Convert-ArrayMatch $match $mapping
    })
    $section = [regex]::Replace($section, $scalarPattern, {
        param([Text.RegularExpressions.Match] $match)
        Convert-ScalarMatch $match $mapping
    })
    return $Source.Text.Substring(0, $Source.SectionStart) + $section + $Source.Text.Substring($Source.SectionEnd)
}

function Update-Offsets {
    [OutputType([string])]
    param([OffsetsSource] $Source, [Row[]] $Rows, [bool] $CanLeaveUnresolved)
    [bool] $hasUnresolved = @($Rows | Where-Object { -not $_.Placed.IsResolved() }).Count -gt 0
    if ($hasUnresolved -and -not $CanLeaveUnresolved) {
        throw 'refusing to apply with unresolved entries (use -AllowUnresolved)'
    }
    [string] $rewritten = Get-RewrittenText $Source $Rows
    [IO.File]::WriteAllText($Source.Path, $rewritten, [Text.UTF8Encoding]::new($false))
    Write-Host 'offsets.h rewritten (clang-format it; bump kVersion* and the panel pin by hand)'
    return $rewritten
}

# Old RVAs quoted in comments are not rewritten; list them for a manual fix.
function Show-OldRvaComments {
    param([string] $Text, [EdgePlus.Re.SymbolTable] $Old)
    [int] $lineNumber = 0
    foreach ($line in $Text -split "`n") {
        $lineNumber++
        if (-not $line.Contains('//')) {
            continue
        }
        [string] $comment = $line.Substring($line.IndexOf('//') + 2)
        foreach ($literal in [regex]::Matches($comment, $hexPattern)) {
            [uint64] $value = ConvertFrom-HexLiteral $literal.Value
            $isOldRva = $value -gt $minimumCommentRva -and $Old.HasSymbolAt($value)
            if ($isOldRva) {
                Write-Host ('  comment line {0}: {1} is an old RVA ({2})' -f $lineNumber, $literal.Value, (Limit-Text (Get-UndecoratedName $Old $value) $symbolLabelLimit))
            }
        }
    }
}

# --- main ----------------------------------------------------------------------

[OffsetsSource] $source = Read-OffsetsSource $Offsets
[EdgePlus.Re.SymbolTable] $old = Import-Symbols $OldPdb
[EdgePlus.Re.SymbolTable] $new = Import-Symbols $NewPdb
[byte[]] $newImage = Read-MappedImage $NewDll
[byte[]] $oldImage = if ($OldDll -and (Test-Path $OldDll)) { Read-MappedImage $OldDll } else { $null }

[Row[]] $rows = New-Rows @(Read-Entries $source.Section) (Read-Overrides $Override) $old $new $newImage $oldImage
Write-Summary $rows

if ($Table) {
    Write-Table $rows $Table
}

[string] $finalText = $source.Text
if ($Apply) {
    $finalText = Update-Offsets $source $rows $AllowUnresolved.IsPresent
}

Show-OldRvaComments $finalText $old
