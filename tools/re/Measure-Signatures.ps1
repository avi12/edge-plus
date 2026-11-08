<#
.SYNOPSIS
  How much of offsets.h do signatures carry from one msedge.dll build to another?

.DESCRIPTION
  For every ordered pair (source, target) of the builds, every `rva` entry of the
  source's offsets.h that is a function start (per the image's own exception table),
  and then every patch site, gets a signature from the source image; the target image
  is scanned for it and the hit is compared with the entry's value in the target's own
  offsets.h, which was verified live. Each entry ends up in exactly one bucket:

    correct    one hit, the right one
    wrong      one hit, not the right one (a signature that would corrupt Edge)
    guarded    one hit the DLL's own checks refuse (right or wrong)
    ambiguous  several hits
    missing    no hit
    unsigned   no unique signature even in the source image

  per level (relocation-aware, tolerant), "layered" (tolerant only where strict is
  not correct) and "vote" (every window and level pooled, as the DLL resolves).

.PARAMETER Build
  "label=<msedge.dll>,<offsets.h of that build>", two or more.
.PARAMETER Out
  The JSON report to write.

.EXAMPLE
  pwsh tools\re\Measure-Signatures.ps1 -Out report.json -Build '156.50=...\156.0.4314.50\msedge.dll,old\offsets.h', '156.58=...\156.0.4314.58\msedge.dll,app\dll\src\edge\offsets.h'
#>
param(
    [Parameter(Mandatory)]
    [string[]] $Build,
    [Parameter(Mandatory)]
    [string] $Out
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Signatures.psm1') -Force
Initialize-Signatures

[int] $sizeTolerancePercent = 25
[int] $sizeSlack = 16
[int] $percent = 100
[int] $maximumTies = 8
[int] $reportDepth = 8
[string[]] $levelNames = @('relocation-aware', 'tolerant')
$levels = @{
    'relocation-aware' = [EdgePlus.Re.SignatureLevel]::RelocationAware
    'tolerant'         = [EdgePlus.Re.SignatureLevel]::Tolerant
}

class MeasuredBuild {
    [string] $Label
    [object] $Image     # EdgePlus.Re.PeImage
    [object] $Scanner   # EdgePlus.Re.SignatureScanner
    [System.Collections.Specialized.OrderedDictionary] $Entries   # name -> rva
}

class Verdict {
    [string] $Name
    [Nullable[uint32]] $Hit
}

function Read-Build {
    [OutputType([MeasuredBuild])]
    param([string] $Specification)
    $label, $paths = $Specification -split '=', 2
    $dll, $offsets = $paths -split ',', 2
    $image = [EdgePlus.Re.PeImage]::new((Resolve-Path $dll).Path)
    $entries = [ordered]@{}
    foreach ($entry in Get-OffsetEntries (Read-SourceText $offsets)) {
        $entries[$entry.Name] = $entry.Rva
    }
    return [MeasuredBuild]@{
        Label   = $label
        Image   = $image
        Scanner = [EdgePlus.Re.SignatureScanner]::new($image)
        Entries = $entries
    }
}

# {name: Located or $null} for the entries of one kind.
function Get-Signatures {
    [OutputType([System.Collections.Specialized.OrderedDictionary])]
    param([MeasuredBuild] $Source, [string] $Kind, [EdgePlus.Re.SignatureLevel] $Level)
    $result = [ordered]@{}
    foreach ($name in $Source.Entries.Keys) {
        $rva = $Source.Entries[$name]
        if ("$(Get-EntryKind $Source.Image $rva)" -ne $Kind) {
            continue
        }
        $maximum = if ($Kind -eq 'Function') { $DefaultMaximumLength } else { $SiteMaximumLength }
        $result[$name] = Find-Signature $Source.Image $Source.Scanner $rva $Level $maximum
    }
    return $result
}

# {name: PlacedProbe or $null}: a second opinion for each function.
function Get-SecondWindows {
    [OutputType([System.Collections.Specialized.OrderedDictionary])]
    param([MeasuredBuild] $Source, [string] $Kind, [EdgePlus.Re.SignatureLevel] $Level, [System.Collections.Specialized.OrderedDictionary] $First)
    $result = [ordered]@{}
    if ($Kind -ne 'Function') {
        return $result
    }
    foreach ($name in $First.Keys) {
        $located = $First[$name]
        $length = if ($null -ne $located) { $located.Signature.Length } else { 0 }
        $result[$name] = Find-SecondWindow $Source.Image $Source.Scanner $Source.Entries[$name] $Level $length
    }
    return $result
}

# What the DLL can check about a hit without knowing the right answer: a function is
# entered at a start the exception table lists, and is about as long as the function
# the signature was made from.
function Test-Guard {
    [OutputType([bool])]
    param([MeasuredBuild] $Target, [string] $Kind, [EdgePlus.Re.Located] $Located, [uint32] $Hit)
    if ($Kind -eq 'Site') {
        return $Target.Image.IsCode($Hit)
    }
    if (-not $Target.Image.FunctionStartSet.Contains($Hit)) {
        return $false
    }
    [int64] $size = $Target.Image.FunctionEnd($Hit) - $Hit
    [int64] $expected = $Located.FunctionSize
    [int64] $allowed = [Math]::Floor($sizeTolerancePercent * $expected / $percent) + $sizeSlack
    return [Math]::Abs($size - $expected) -le $allowed
}

function Get-Verdict {
    [OutputType([Verdict])]
    param([EdgePlus.Re.Located] $Located, [string] $Kind, [MeasuredBuild] $Target, [uint32] $Truth)
    if ($null -eq $Located) {
        return [Verdict]@{ Name = 'unsigned' }
    }
    $hits = $Target.Scanner.FindAll($Located.Signature, $maximumTies + 1)
    if ($hits.Count -eq 0) {
        return [Verdict]@{ Name = 'missing' }
    }
    $hit = $Located.Pick($hits)
    if ($null -eq $hit) {
        return [Verdict]@{ Name = 'ambiguous' }
    }
    if (-not (Test-Guard $Target $Kind $Located $hit)) {
        return [Verdict]@{ Name = 'guarded'; Hit = $hit }
    }
    $name = if ($hit -eq $Truth) { 'correct' } else { 'wrong' }
    return [Verdict]@{ Name = $name; Hit = $hit }
}

# Pool what every signature of an entry found in the target: strong = both windows
# name the same address, weak = one window names it and nothing contradicts it.
function Get-Vote {
    [OutputType([Verdict])]
    param([string] $Name, [hashtable] $ByLevel, [hashtable] $SecondByLevel, [string] $Kind, [MeasuredBuild] $Target, [uint32] $Truth)
    $candidates = [Collections.Generic.Dictionary[uint32, Collections.Generic.HashSet[string]]]::new()
    $isAnySigned = $false
    foreach ($levelName in $levelNames) {
        $first = $ByLevel[$levelName][$Name]
        $second = if ($SecondByLevel[$levelName].Contains($Name)) { $SecondByLevel[$levelName][$Name] } else { $null }
        $hits = [Collections.Generic.List[uint32]]::new()
        if ($null -ne $first) {
            $isAnySigned = $true
            $hits = $Target.Scanner.FindAll($first.Signature, $maximumTies + 1)
            $pick = $first.Pick($hits)
            $isGuardedPick = $null -ne $pick -and (Test-Guard $Target $Kind $first $pick)
            if ($isGuardedPick) {
                if (-not $candidates.ContainsKey($pick)) {
                    $candidates[$pick] = [Collections.Generic.HashSet[string]]::new()
                }
                [void]$candidates[$pick].Add('first')
            }
        }
        if ($null -ne $second) {
            $secondHits = $Target.Scanner.FindAll($second.Located.Signature, 2)
            if ($secondHits.Count -eq 1) {
                [uint32] $candidate = $secondHits[0] - $second.Offset
                $isGuardedCandidate = $null -ne $first -and (Test-Guard $Target $Kind $first $candidate)
                if ($isGuardedCandidate) {
                    if (-not $candidates.ContainsKey($candidate)) {
                        $candidates[$candidate] = [Collections.Generic.HashSet[string]]::new()
                    }
                    [void]$candidates[$candidate].Add('second')
                    if ($hits.Contains($candidate)) {
                        [void]$candidates[$candidate].Add('first')
                    }
                }
            }
        }
    }
    if (-not $isAnySigned) {
        return [Verdict]@{ Name = 'unsigned' }
    }
    if ($candidates.Count -eq 0) {
        return [Verdict]@{ Name = 'missing' }
    }
    if ($candidates.Count -gt 1) {
        return [Verdict]@{ Name = 'conflict' }
    }
    [uint32] $hit = @($candidates.Keys)[0]
    $strength = if ($candidates[$hit].Count -eq 2) { 'strong' } else { 'weak' }
    $outcome = if ($hit -eq $Truth) { 'correct' } else { 'wrong' }
    return [Verdict]@{ Name = "$strength-$outcome"; Hit = $hit }
}

function Format-Hex {
    [OutputType([string])]
    param([Nullable[uint32]] $Value)
    if ($null -eq $Value) {
        return $null
    }
    return '0x{0:x}' -f $Value
}

$builds = @($Build | ForEach-Object { Read-Build $_ })
$pairs = [Collections.Generic.List[object]]::new()
foreach ($kind in 'Function', 'Site') {
    foreach ($source in $builds) {
        $stopwatch = [Diagnostics.Stopwatch]::StartNew()
        $byLevel = @{}
        $secondByLevel = @{}
        foreach ($levelName in $levelNames) {
            $byLevel[$levelName] = Get-Signatures $source $kind $levels[$levelName]
            $secondByLevel[$levelName] = Get-SecondWindows $source $kind $levels[$levelName] $byLevel[$levelName]
        }
        $strict = $byLevel['relocation-aware']
        $signed = @($strict.Values | Where-Object { $null -ne $_ })
        $unique = @($signed | Where-Object { $_.IsUnique })
        Write-Host ('{0} from {1}: {2}/{3} signed, {4} unique ({5:n0}s)' -f $kind.ToLowerInvariant(), $source.Label,
            $signed.Count, $strict.Count, $unique.Count, $stopwatch.Elapsed.TotalSeconds)
        foreach ($target in $builds) {
            if ($target -eq $source) {
                continue
            }
            $results = [ordered]@{}
            foreach ($levelName in @($levelNames) + 'layered', 'vote') {
                $buckets = [ordered]@{}
                $details = [ordered]@{}
                foreach ($name in $strict.Keys) {
                    if (-not $target.Entries.Contains($name)) {
                        $buckets['no ground truth'] = 1 + ($buckets['no ground truth'] ?? 0)
                        continue
                    }
                    [uint32] $truth = $target.Entries[$name]
                    switch ($levelName) {
                        'vote' {
                            $verdict = Get-Vote $name $byLevel $secondByLevel $kind $target $truth
                            if ($verdict.Name -eq 'strong-correct') {
                                $verdict.Name = 'correct'
                            }
                        }
                        'layered' {
                            $verdict = Get-Verdict $byLevel['relocation-aware'][$name] $kind $target $truth
                            if ($verdict.Name -ne 'correct') {
                                $verdict = Get-Verdict $byLevel['tolerant'][$name] $kind $target $truth
                            }
                        }
                        default {
                            $verdict = Get-Verdict $byLevel[$levelName][$name] $kind $target $truth
                        }
                    }
                    $buckets[$verdict.Name] = 1 + ($buckets[$verdict.Name] ?? 0)
                    if ($verdict.Name -ne 'correct') {
                        $details[$name] = @($verdict.Name, (Format-Hex $verdict.Hit), (Format-Hex $truth))
                    }
                }
                $results[$levelName] = [ordered]@{ buckets = $buckets; not_correct = $details }
                Write-Host ('  -> {0} {1}: {2}' -f $target.Label, $levelName,
                    (($buckets.Keys | ForEach-Object { "$_ $($buckets[$_])" }) -join ', '))
            }
            $lengths = @($signed | ForEach-Object { $_.Signature.Length })
            $average = if ($lengths.Count -gt 0) { ($lengths | Measure-Object -Average).Average } else { 0 }
            $pairs.Add([ordered]@{
                    kind           = $kind.ToLowerInvariant()
                    source         = $source.Label
                    target         = $target.Label
                    entries        = $strict.Count
                    average_length = $average
                    ordinal_signed = @($signed | Where-Object { -not $_.IsUnique }).Count
                    results        = $results
                })
        }
    }
}
$report = [ordered]@{ builds = @($builds | ForEach-Object { $_.Label }); pairs = $pairs }
$report | ConvertTo-Json -Depth $reportDepth | Set-Content -Encoding utf8NoBOM $Out
Write-Host "wrote $Out"
