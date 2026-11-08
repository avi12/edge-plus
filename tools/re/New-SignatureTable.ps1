<#
.SYNOPSIS
  Write the signature table the DLL falls back on when msedge.dll is not the build
  offsets.h is pinned to: app/dll/src/edge/signatures.g.h.

.DESCRIPTION
  Every `rva` entry of -Offsets becomes a table entry, keyed by the value the same
  name has in -KeyOffsets (the pinned offsets.h; the default is -Offsets itself, the
  normal case: the dll is the pinned build). In the DLL, edge::At(pinned) is then
  translated to wherever the entry's signatures find it in the running msedge.dll.

    function  up to four probes: a strict and a tolerant signature of the function's
              opening, and of a window further in, as a second opinion
    callee    a function with no signature of its own: the probes of a function that
              calls it, and where in that function the call's rel32 is
    site      a probe at the patch site itself
    inside    a site with no usable bytes of its own: a fixed distance into the
              function that holds it
    data      the probes of a function that refers to it (or to data beside it), and
              where in that function the RIP-relative displacement is

  Each entry also carries the features (the panel's switches) that use it, so a
  feature whose entries do not all resolve turns off alone. The feature bits are read
  from signature_table.h; which banner of offsets.h (or which name) maps to which
  feature is EdgePlus.Signatures.psm1's.

.PARAMETER Dll
  The msedge.dll to sign. Default: the pinned build's (kVersion* of -Offsets).
.PARAMETER Offsets
  The offsets.h of that dll.
.PARAMETER KeyOffsets
  The offsets.h whose names and values key the table (default: -Offsets).
.PARAMETER Version
  The dll's version, for the header (default: its file version).
.PARAMETER Out
  The table to write.
.PARAMETER Check
  Only verify that -Out lists exactly -Offsets' entries; exit 1 when it does not.

.EXAMPLE
  pwsh tools\re\New-SignatureTable.ps1
.EXAMPLE
  pwsh tools\re\New-SignatureTable.ps1 -Check
#>
param(
    [string] $Dll,
    [string] $Offsets = (Join-Path $PSScriptRoot '..\..\app\dll\src\edge\offsets.h'),
    [string] $KeyOffsets,
    [string] $Version,
    [string] $Out = (Join-Path $PSScriptRoot '..\..\app\dll\src\edge\signatures.g.h'),
    [switch] $Check
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Signatures.psm1') -Force

[string] $applicationFolder = 'C:\Program Files (x86)\Microsoft\Edge\Application'
[string] $signatureTableHeader = Join-Path $PSScriptRoot '..\..\app\dll\src\edge\signature_table.h'
[string] $versionPattern = 'inline constexpr int kVersion(Major|Minor|Build|Revision)\s*=\s*(\d+);'
[string] $tableNamespace = 'edgeplus::edge155::signatures'

if ($Check) {
    $isFresh = Test-SignatureTable $Offsets $Out
    if ($isFresh) {
        exit 0
    }
    exit 1
}

$stopwatch = [Diagnostics.Stopwatch]::StartNew()
$sourceText = Read-SourceText $Offsets
if (-not $Dll) {
    $parts = @{}
    foreach ($match in [regex]::Matches($sourceText, $versionPattern)) {
        $parts[$match.Groups[1].Value] = $match.Groups[2].Value
    }
    $pinned = '{0}.{1}.{2}.{3}' -f $parts['Major'], $parts['Minor'], $parts['Build'], $parts['Revision']
    $Dll = Join-Path $applicationFolder "$pinned\msedge.dll"
}
if (-not (Test-Path $Dll)) {
    throw "no msedge.dll at $Dll"
}
if (-not $Version) {
    $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($Dll)
    $Version = '{0}.{1}.{2}.{3}' -f $info.FileMajorPart, $info.FileMinorPart, $info.FileBuildPart, $info.FilePrivatePart
}

Initialize-Signatures
$bits = Get-FeatureBits $signatureTableHeader
$image = [EdgePlus.Re.PeImage]::new((Resolve-Path $Dll).Path)
$scanner = [EdgePlus.Re.SignatureScanner]::new($image)
$keyText = if ($KeyOffsets) { Read-SourceText $KeyOffsets } else { $sourceText }
$keys = @{}
foreach ($entry in Get-OffsetEntries $keyText) {
    $keys[$entry.Name] = $entry.Rva
}
$entries = @(Get-OffsetEntries $sourceText | Where-Object { $keys.ContainsKey($_.Name) })
Write-Host "$($entries.Count) entries of $Offsets"

$kinds = @{}
foreach ($entry in $entries) {
    $kinds[$entry.Name] = Get-EntryKind $image $entry.Rva
}
# Data nothing in code refers to may sit beside data something does; look at the
# addresses around every datum too, nearest first and the lower one first.
[int[]] $neighbourSteps = for ($step = -$NeighbourReach; $step -le $NeighbourReach; $step += $NeighbourStep) {
    $step
}
$neighbourSteps = @($neighbourSteps | Sort-Object -Stable { [Math]::Abs($_) })
$dataTargets = [Collections.Generic.HashSet[uint32]]::new()
$codeTargets = [Collections.Generic.HashSet[uint32]]::new()
foreach ($entry in $entries) {
    if ($kinds[$entry.Name] -eq 'Data') {
        foreach ($step in $neighbourSteps) {
            [void]$dataTargets.Add([uint32]($entry.Rva + $step))
        }
    } else {
        [void]$codeTargets.Add($entry.Rva)
    }
}
$references = [EdgePlus.Re.CodeReferences]::RipReferences($image, $dataTargets)
$callSites = [EdgePlus.Re.CodeReferences]::CallReferences($image, $codeTargets)

$lines = [Collections.Generic.List[string]]::new()
$totals = [ordered]@{ Function = 0; Site = 0; Data = 0 }
$signed = [ordered]@{ Function = 0; Site = 0; Data = 0 }
$rowKinds = [ordered]@{ kFunction = 0; kCallee = 0; kSite = 0; kInside = 0; kData = 0 }
[int] $byCaller = 0
foreach ($entry in $entries) {
    $name = $entry.Name
    $rva = $entry.Rva
    $kind = $kinds[$name]
    $features = Get-EntryFeatures $name $entry.Section $bits
    $pinned = $keys[$name]
    $totals["$kind"]++
    [string] $row = $null
    [string] $rowKind = $null
    switch ($kind) {
        'Function' {
            $probes = Get-Probes $image $scanner $rva $DefaultMaximumLength
            [uint32] $size = $image.FunctionEnd($rva) - $rva
            if ($probes.Count -gt 0) {
                $signed.Function++
                $rowKind = 'kFunction'
                $row = Format-Entry $pinned $rowKind $features $size 0 0 $probes $name
                break
            }
            $calleeHost = Find-CalleeHost $image $scanner $callSites $rva
            if ($null -eq $calleeHost) {
                $rowKind = 'kFunction'
                $row = Format-Entry $pinned $rowKind $features $size 0 0 @() $name
                break
            }
            $signed.Function++
            $byCaller++
            $rowKind = 'kCallee'
            $row = Format-Entry $pinned $rowKind $features $calleeHost.Size $calleeHost.OperandOffset 0 $calleeHost.Probes $name `
                -TargetSize $size -TargetPattern (Get-TargetPattern $image $rva)
        }
        'Site' {
            $probes = @((Get-Probes $image $scanner $rva $SiteMaximumLength) | Where-Object { $_.Offset -eq 0 })
            if ($probes.Count -gt 0) {
                $signed.Site++
                $rowKind = 'kSite'
                $row = Format-Entry $pinned $rowKind $features 0 0 0 $probes $name
                break
            }
            # A leaf thunk has no unwind entry, so it reads as a site; it can still be
            # the target of a call.
            $calleeHost = Find-CalleeHost $image $scanner $callSites $rva
            if ($null -ne $calleeHost) {
                $signed.Site++
                $byCaller++
                $rowKind = 'kCallee'
                $row = Format-Entry $pinned $rowKind $features $calleeHost.Size $calleeHost.OperandOffset 0 $calleeHost.Probes $name `
                    -TargetSize 0 -TargetPattern (Get-TargetPattern $image $rva)
                break
            }
            # Cold code and return addresses: a fixed distance into the function that
            # holds them, when that function has a signature.
            $functionStart = $image.FunctionStart($rva)
            $inside = @()
            if ($null -ne $functionStart) {
                $inside = Get-Probes $image $scanner $functionStart $DefaultMaximumLength
            }
            if ($inside.Count -gt 0) {
                $signed.Site++
                $rowKind = 'kInside'
                $row = Format-Entry $pinned $rowKind $features ($image.FunctionEnd($rva) - $functionStart) ($rva - $functionStart) 0 $inside $name
                break
            }
            $rowKind = 'kSite'
            $row = Format-Entry $pinned $rowKind $features 0 0 0 @() $name
        }
        'Data' {
            $dataHost = $null
            [int] $delta = 0
            foreach ($step in $neighbourSteps) {
                $dataHost = Find-DataHost $image $scanner $references ([uint32]($rva + $step))
                if ($null -ne $dataHost) {
                    $delta = -$step
                    break
                }
            }
            $rowKind = 'kData'
            if ($null -eq $dataHost) {
                $row = Format-Entry $pinned $rowKind $features 0 0 0 @() $name
                break
            }
            $signed.Data++
            $row = Format-Entry $pinned $rowKind $features $dataHost.Size $dataHost.OperandOffset $dataHost.OperandExtra $dataHost.Probes $name `
                -DataDelta $delta
        }
    }
    $rowKinds[$rowKind]++
    $lines.Add($row)
}
$counts = foreach ($kind in $totals.Keys) {
    '{0} {1}/{2}' -f $kind.ToLowerInvariant(), $signed[$kind], $totals[$kind]
}
Write-Host ("signed: {0} (of the functions and sites, {1} by a caller)" -f ($counts -join ', '), $byCaller)
Write-Host ('rows: ' + (($rowKinds.Keys | ForEach-Object { "$_ $($rowKinds[$_])" }) -join ', '))

$text = [Text.StringBuilder]::new()
[void]$text.Append("// signatures.g.h - GENERATED by tools/re/New-SignatureTable.ps1; do not edit.`n")
[void]$text.Append("// Signed from msedge.dll $Version; keyed by the RVAs of offsets.h.`n")
[void]$text.Append("#pragma once`n#include `"edge/signature_table.h`"`n`n")
[void]$text.Append("// clang-format off`n")
[void]$text.Append("namespace $tableNamespace {`n`n")
[void]$text.Append("inline constexpr char kSignedBuild[] = `"$Version`";`n`n")
[void]$text.Append("inline constexpr Entry kEntries[] = {`n")
[void]$text.Append(($lines -join "`n"))
[void]$text.Append("`n};`n`n}  // namespace $tableNamespace`n")
[void]$text.Append("// clang-format on`n")
[IO.File]::WriteAllText([IO.Path]::GetFullPath($Out), $text.ToString(), [Text.UTF8Encoding]::new($false))
Write-Host ('wrote {0} in {1:n1} s' -f $Out, $stopwatch.Elapsed.TotalSeconds)
