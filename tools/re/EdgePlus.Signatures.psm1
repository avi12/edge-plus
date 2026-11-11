# The signature table's rules, shared by New-SignatureTable.ps1 (which writes
# app/dll/src/edge/signatures.g.h) and Measure-Signatures.ps1: which offsets.h entries
# there are, which features use each, and how an entry's probes are chosen. The byte
# work (the image, masking, scanning, references) is Signatures.cs.

Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Re.psm1')

[string] $script:EntryPattern = 'inline constexpr uintptr_t\s+(\w+)(\[\])?\s*=\s*(?:kImageBase\s*\+\s*)?(0x[0-9a-fA-F]+|\{[^}]*\})\s*;'
[string] $script:HexPattern = '0x[0-9a-fA-F]+'
# The `// --- title` banner of offsets.h: the title stops at a parenthesis or the
# closing dashes.
[string] $script:BannerPattern = '(?m)^// --- (.*?)(?: \(.*)?(?: ---)?$'
[string] $script:FeatureBitPattern = 'kFeatureBit(\w+)\s*=\s*(\d+);'
[string] $script:TableEntryPattern = '(?m)^\s*\{0x([0-9a-f]+), "[^"]*", "([^"]+)", \{\{'
[string] $script:ArrayIndexSuffix = '\[\d+\]$'
[string] $script:RvaNamespaceStart = 'namespace rva {'
[string] $script:RvaNamespaceEnd = 'namespace off {'

[int] $script:DefaultMaximumLength = 256
[int] $script:SiteMaximumLength = 64
[int] $script:MaximumTies = 8
[int] $script:ProbesPerEntry = 4
[int] $script:SecondWindowLength = 64
[int] $script:SecondWindowMinimumTail = 16
[int] $script:SecondWindowFarLimit = 1024
# Bytes decoded past the far window, so the instruction there is whole.
[int] $script:SecondWindowSlack = 64
[int] $script:TargetPatternLength = 12
# Data laid out this close to other data is taken to move with it; the step is the
# alignment of the objects (pointers, VectorIcons).
[int] $script:NeighbourReach = 0x100
[int] $script:NeighbourStep = 8

enum EntryKind {
    Function   # the exact start of a function the exception table lists
    Site       # inside code: a patch site or a cold-code address
    Data       # outside code: a vtable, a global, a table
}

class OffsetEntry {
    [string] $Name
    [uint32] $Rva
    [string] $Section
}

class FeatureRule {
    [string] $Match       # a banner title prefix, or a name pattern
    [string[]] $Features
}

# The banner of offsets.h an entry sits under -> the features that use it. A banner
# not listed here is shared by every feature (the safe default).
[FeatureRule[]] $script:SectionFeatures = @(
    [FeatureRule]@{ Match = 'function RVAs'; Features = @('Split') }
    [FeatureRule]@{ Match = 'pane construction'; Features = @('Split') }
    [FeatureRule]@{ Match = 'tab wiring'; Features = @('Split') }
    [FeatureRule]@{ Match = 'the feed pipeline'; Features = @('Split') }
    [FeatureRule]@{ Match = 'native split data-model grow'; Features = @('Split') }
    [FeatureRule]@{ Match = "the split's 3-dot menu"; Features = @('Split') }
    [FeatureRule]@{ Match = 'collapse'; Features = @('Split') }
    [FeatureRule]@{ Match = 'per-pane hover controls'; Features = @('Split') }
    [FeatureRule]@{ Match = 'the drag grip'; Features = @('Split') }
    [FeatureRule]@{ Match = 'closing one pane'; Features = @('Split') }
    [FeatureRule]@{ Match = 'the dividers between panes'; Features = @('Split') }
    [FeatureRule]@{ Match = '2-pane-only CHECK sites'; Features = @('Split') }
    [FeatureRule]@{ Match = "a split's tabs in the tab list"; Features = @('Split') }
    [FeatureRule]@{ Match = 'theming'; Features = @('Theming') }
    [FeatureRule]@{ Match = "reading mode's page theme"; Features = @('ReadingTheme') }
    [FeatureRule]@{ Match = 'WebUI pages follow the theme'; Features = @('WebUiFollowsTheme') }
    [FeatureRule]@{ Match = 'web protocol handlers'; Features = @('ProtocolHandlers') }
    [FeatureRule]@{ Match = "the browser assistant's toolbar button"; Features = @('EnhancedFind') }
    [FeatureRule]@{ Match = 'running a script in a page, and reading a tab'; Features = @('EnhancedFind') }
    [FeatureRule]@{ Match = 'live tab hover preview'; Features = @('LivePreview') }
)
# Names that belong to one feature whatever banner they sit under: the Cyberpunk
# skin's tab outline, tab and omnibox frames and fonts.
[FeatureRule[]] $script:NameFeatures = @(
    [FeatureRule]@{
        Match    = '^(EdgeTabStyleViews_|SkPath_|SkRRect_|Canvas_DrawRoundRect|Tab_IsActive|EdgeTab_IsActive|TabStrip_GetStrokeThickness|LocationBarView_|BubbleBorder_|Font_|FontList_|Label_SetFontList)'
        Features = @('CyberpunkSkin')
    }
)

# The DLL's source files that belong to one feature (paths under app/dll/src,
# a folder ending in /). Usage adds to the banner rules above, never takes away:
# an entry a feature's code reaches (directly, or through edge.h's helpers)
# carries that feature, so the feature stands down when the entry is missing.
# Files not listed (edge.h, hooks.h, control.h, core/) are shared.
[FeatureRule[]] $script:FileFeatures = @(
    [FeatureRule]@{ Match = 'split/'; Features = @('Split') }
    [FeatureRule]@{ Match = 'layout/'; Features = @('Split') }
    [FeatureRule]@{ Match = 'chrome/pane/'; Features = @('Split') }
    [FeatureRule]@{ Match = 'chrome/tabs/tabdrag.h'; Features = @('Split') }
    [FeatureRule]@{ Match = 'chrome/tabs/tabrows.h'; Features = @('Split') }
    [FeatureRule]@{ Match = 'chrome/tabs/tab_views.h'; Features = @('Split') }
    [FeatureRule]@{ Match = 'chrome/tabs/tab_shape.h'; Features = @('CyberpunkSkin') }
    [FeatureRule]@{ Match = 'chrome/tabs/tab_frame.h'; Features = @('CyberpunkSkin') }
    [FeatureRule]@{ Match = 'chrome/tabs/tab_font.h'; Features = @('CyberpunkSkin') }
    [FeatureRule]@{ Match = 'chrome/toolbar/'; Features = @('CyberpunkSkin') }
    [FeatureRule]@{ Match = 'chrome/menu/'; Features = @('CyberpunkSkin') }
    [FeatureRule]@{ Match = 'chrome/tabs/live_preview.h'; Features = @('LivePreview') }
    [FeatureRule]@{ Match = 'theme/cyberpunk_skin.h'; Features = @('CyberpunkSkin') }
    [FeatureRule]@{ Match = 'theme/skin_tone.h'; Features = @('CyberpunkSkin') }
    [FeatureRule]@{ Match = 'theme/theme.h'; Features = @('Theming') }
    [FeatureRule]@{ Match = 'theme/theme_notice.h'; Features = @('Theming') }
    [FeatureRule]@{ Match = 'theme/protocol_handlers.h'; Features = @('ProtocolHandlers') }
    [FeatureRule]@{ Match = 'theme/theme_follow.h'; Features = @('WebUiFollowsTheme') }
    [FeatureRule]@{ Match = 'theme/followed_pages.h'; Features = @('WebUiFollowsTheme') }
    [FeatureRule]@{ Match = 'theme/reading_'; Features = @('ReadingTheme') }
    [FeatureRule]@{ Match = 'find/'; Features = @('EnhancedFind') }
    [FeatureRule]@{ Match = 'assistant/'; Features = @('EnhancedFind') }
)
[string] $script:RvaUsePattern = 'rva::(\w+)'
[string] $script:CallPattern = '\b([A-Z]\w+)\('
# hooks.h's `Install(Feature::kX, edge155::rva::Name, reinterpret_cast<void*>(detour)`.
[string] $script:TaggedInstallPattern = '(?s)Install\(\s*Feature::k(\w+),\s*edge155::rva::(\w+),\s*reinterpret_cast<void\*>\(\s*(?:\w+::)*(\w+)\)'
# A top-level `inline` function of a binding header, up to its closing brace at
# the start of a line.
[string] $script:TopLevelFunctionPattern = '(?ms)^(?:template\s*<[^>]*>\s*)?(?:\[\[nodiscard\]\]\s*)?inline\s+[^;{]*?\b(\w+)\s*\([^;{]*?\)\s*(?:const\s*)?(?:noexcept\s*)?\{.*?^\}'

# {entry base name: feature bits} of what each feature's own code reaches.
function Get-UsageFeatures {
    [OutputType([hashtable])]
    param([string] $SourceRoot, [System.Collections.Specialized.OrderedDictionary] $Bits)
    $root = (Resolve-Path $SourceRoot).Path
    $files = Get-ChildItem $root -Recurse -Include '*.h', '*.cpp' |
        Where-Object { $_.Name -notin 'offsets.h', 'signatures.g.h' }
    # edge.h's helpers: name -> (entries it names, helpers it calls).
    $helperEntries = @{}
    $helperCalls = @{}
    $bindingText = Read-SourceText (Join-Path $root 'edge\edge.h')
    foreach ($function in [regex]::Matches($bindingText, $script:TopLevelFunctionPattern)) {
        $name = $function.Groups[1].Value
        $body = $function.Value
        $helperEntries[$name] = @([regex]::Matches($body, $script:RvaUsePattern) | ForEach-Object { $_.Groups[1].Value })
        $helperCalls[$name] = @([regex]::Matches($body, $script:CallPattern) | ForEach-Object { $_.Groups[1].Value } |
            Where-Object { $_ -ne $name } | Select-Object -Unique)
    }
    $usage = @{}
    # Reach every entry `text` names, directly or through edge.h's helpers.
    $addReach = {
        param([string] $Text, [int] $FeatureBits)
        $reached = [Collections.Generic.HashSet[string]]::new()
        foreach ($use in [regex]::Matches($Text, $script:RvaUsePattern)) {
            [void]$reached.Add($use.Groups[1].Value)
        }
        $pending = [Collections.Generic.Queue[string]]::new()
        $visited = [Collections.Generic.HashSet[string]]::new()
        foreach ($call in [regex]::Matches($Text, $script:CallPattern)) {
            $pending.Enqueue($call.Groups[1].Value)
        }
        while ($pending.Count -gt 0) {
            $helper = $pending.Dequeue()
            $isNewHelper = $helperEntries.ContainsKey($helper) -and $visited.Add($helper)
            if (-not $isNewHelper) {
                continue
            }
            foreach ($entry in $helperEntries[$helper]) {
                [void]$reached.Add($entry)
            }
            foreach ($callee in $helperCalls[$helper]) {
                $pending.Enqueue($callee)
            }
        }
        foreach ($entry in $reached) {
            $usage[$entry] = [int]$usage[$entry] -bor $FeatureBits
        }
    }
    # hooks.h tags each Install with its feature: the hooked entry, and what the
    # detour (when hooks.h defines it) reaches, belong to that feature.
    $hooksText = Read-SourceText (Join-Path $root 'edge\hooks.h')
    $detours = @{}
    foreach ($function in [regex]::Matches($hooksText, $script:TopLevelFunctionPattern)) {
        $detours[$function.Groups[1].Value] = $function.Value
    }
    foreach ($install in [regex]::Matches($hooksText, $script:TaggedInstallPattern)) {
        $featureBits = [int]$Bits[$install.Groups[1].Value]
        $usage[$install.Groups[2].Value] = [int]$usage[$install.Groups[2].Value] -bor $featureBits
        $detour = $install.Groups[3].Value
        if ($detours.ContainsKey($detour)) {
            & $addReach $detours[$detour] $featureBits
        }
    }
    foreach ($file in $files) {
        $relative = $file.FullName.Substring($root.Length + 1).Replace('\', '/')
        [string[]] $features = $null
        foreach ($rule in $script:FileFeatures) {
            if ($relative.StartsWith($rule.Match, [StringComparison]::Ordinal)) {
                $features = $rule.Features
                break
            }
        }
        if (-not $features) {
            continue
        }
        [int] $featureBits = 0
        foreach ($feature in $features) {
            $featureBits = $featureBits -bor $Bits[$feature]
        }
        & $addReach (Read-SourceText $file.FullName) $featureBits
    }
    return $usage
}

# The byte work, compiled once against the Iced decoder.
function Initialize-Signatures {
    [OutputType([void])]
    param()
    if ('EdgePlus.Re.SignatureScanner' -as [type]) {
        return
    }
    Initialize-Iced
    # Naming any reference drops Add-Type's defaults, so the framework's are named too.
    [string[]] $references = @(
        [Iced.Intel.Decoder].Assembly.Location
        'netstandard', 'System.Runtime', 'System.Collections', 'System.Memory', 'System.Runtime.Extensions'
    )
    Add-Type -Path (Join-Path $PSScriptRoot 'Signatures.cs') -ReferencedAssemblies $references
}

# A source file as Python's text mode reads it: CRLF folded to LF, so positions and
# `$` match the same way on either checkout.
function Read-SourceText {
    [OutputType([string])]
    param([string] $Path)
    return [IO.File]::ReadAllText($Path).Replace("`r`n", "`n")
}

# {name: bit} of the DLL's Feature enum, read from the header that owns the numbers.
function Get-FeatureBits {
    [OutputType([System.Collections.Specialized.OrderedDictionary])]
    param([string] $Header)
    $bits = [ordered]@{}
    foreach ($match in [regex]::Matches((Read-SourceText $Header), $script:FeatureBitPattern)) {
        $bits[$match.Groups[1].Value] = [int]$match.Groups[2].Value
    }
    if ($bits.Count -eq 0) {
        throw "no kFeatureBit constants in $Header"
    }
    return $bits
}

# Every scalar and array element of offsets.h's `rva` namespace, with the banner it
# sits under.
function Get-OffsetEntries {
    [OutputType([OffsetEntry])]
    param([string] $Text)
    $start = $Text.IndexOf($script:RvaNamespaceStart)
    $end = $Text.IndexOf($script:RvaNamespaceEnd)
    $banners = [regex]::Matches($Text, $script:BannerPattern)
    $regex = [regex]::new($script:EntryPattern)
    $match = $regex.Match($Text, $start, $end - $start)
    while ($match.Success) {
        [string] $section = $null
        foreach ($banner in $banners) {
            $isBefore = $banner.Index + $banner.Length -le $match.Index
            if (-not $isBefore) {
                break
            }
            $section = $banner.Groups[1].Value.Trim()
        }
        $name = $match.Groups[1].Value
        $value = $match.Groups[3].Value
        if ($match.Groups[2].Success) {
            $position = 0
            foreach ($element in [regex]::Matches($value, $script:HexPattern)) {
                [OffsetEntry]@{ Name = "$name[$position]"; Rva = [Convert]::ToUInt32($element.Value, 16); Section = $section }
                $position++
            }
        } else {
            [OffsetEntry]@{ Name = $name; Rva = [Convert]::ToUInt32($value, 16); Section = $section }
        }
        $match = $match.NextMatch()
    }
}

function Get-EntryFeatures {
    [OutputType([int])]
    param([string] $Name, [string] $Section, [System.Collections.Specialized.OrderedDictionary] $Bits)
    $base = $Name -replace $script:ArrayIndexSuffix, ''
    [string[]] $features = $null
    foreach ($rule in $script:NameFeatures) {
        if ($base -cmatch $rule.Match) {
            $features = $rule.Features
            break
        }
    }
    if (-not $features) {
        foreach ($rule in $script:SectionFeatures) {
            $isUnderBanner = $Section -and $Section.StartsWith($rule.Match, [StringComparison]::Ordinal)
            if ($isUnderBanner) {
                $features = $rule.Features
                break
            }
        }
    }
    if (-not $features) {
        $features = [string[]]$Bits.Keys
    }
    [int] $sum = 0
    foreach ($feature in $features) {
        $sum += $Bits[$feature]
    }
    return $sum
}

function Get-EntryKind {
    [OutputType([EntryKind])]
    param([EdgePlus.Re.PeImage] $Image, [uint32] $Rva)
    if ($Image.FunctionStartSet.Contains($Rva)) {
        return [EntryKind]::Function
    }
    if ($Image.IsCode($Rva)) {
        return [EntryKind]::Site
    }
    return [EntryKind]::Data
}

# The shortest signature that finds the code at Rva, never running past the end of
# its function (the bytes after it are the next function's); $null when no
# signature picks it out.
function Find-Signature {
    [OutputType([EdgePlus.Re.Located])]
    param(
        [EdgePlus.Re.PeImage] $Image,
        [EdgePlus.Re.SignatureScanner] $Scanner,
        [uint32] $Rva,
        [EdgePlus.Re.SignatureLevel] $Level = [EdgePlus.Re.SignatureLevel]::RelocationAware,
        [int] $MaximumLength = $script:DefaultMaximumLength
    )
    $end = $Image.FunctionEnd($Rva)
    [int] $extent = if ($null -ne $end) { $end - $Rva } else { $MaximumLength }
    [int] $length = [Math]::Min($MaximumLength, $extent)
    $code = $Image.Read($Rva, $length + [EdgePlus.Re.SignatureDecoder]::InstructionSlack)
    $full = [EdgePlus.Re.SignatureDecoder]::MaskedBytes($code, $Level, $length)
    if ($null -eq $full) {
        return $null
    }
    $found = $Scanner.ShortestUnique($full, $Rva)
    if ($null -eq $found) {
        return $null
    }
    $total = $found.Ties.Count + 1
    if ($total -gt $script:MaximumTies) {
        return $null
    }
    [uint32[]] $places = @($found.Ties) + $Rva | Sort-Object
    return [EdgePlus.Re.Located]@{
        Signature    = $found.Signature
        Ordinal      = [Array]::IndexOf($places, $Rva)
        Total        = $total
        FunctionSize = $extent
    }
}

# An independent signature of the same function, taken from its second half, for a
# second opinion on a hit of the first; $null when the function is too short to have
# one.
function Find-SecondWindow {
    [OutputType([EdgePlus.Re.PlacedProbe])]
    param(
        [EdgePlus.Re.PeImage] $Image,
        [EdgePlus.Re.SignatureScanner] $Scanner,
        [uint32] $Rva,
        [EdgePlus.Re.SignatureLevel] $Level,
        [int] $FirstLength
    )
    $end = $Image.FunctionEnd($Rva)
    if ($null -eq $end) {
        return $null
    }
    [int] $size = $end - $Rva
    [int] $wanted = [Math]::Max($FirstLength, [Math]::Min([Math]::Floor($size / 2), $script:SecondWindowFarLimit))
    $code = $Image.Read($Rva, [Math]::Min($size, $wanted + $script:SecondWindowSlack))
    $position = [EdgePlus.Re.SignatureDecoder]::InstructionAtOrAfter($code, $wanted)
    if ($position -lt 0) {
        return $null
    }
    if ($size - $position -lt $script:SecondWindowMinimumTail) {
        return $null
    }
    $located = Find-Signature $Image $Scanner ($Rva + $position) $Level $script:SecondWindowLength
    $isUsable = $null -ne $located -and $located.IsUnique
    if (-not $isUsable) {
        return $null
    }
    return [EdgePlus.Re.PlacedProbe]@{ Offset = $position; Located = $located }
}

# Up to four probes of the code at Rva: strict then tolerant, the opening then a
# window further in.
function Get-Probes {
    [OutputType([EdgePlus.Re.PlacedProbe])]
    param(
        [EdgePlus.Re.PeImage] $Image,
        [EdgePlus.Re.SignatureScanner] $Scanner,
        [uint32] $Rva,
        [int] $MaximumLength
    )
    $probes = [Collections.Generic.List[EdgePlus.Re.PlacedProbe]]::new()
    $seen = [Collections.Generic.HashSet[string]]::new()
    foreach ($level in [EdgePlus.Re.SignatureLevel]::RelocationAware, [EdgePlus.Re.SignatureLevel]::Tolerant) {
        $first = Find-Signature $Image $Scanner $Rva $level $MaximumLength
        if ($null -eq $first) {
            continue
        }
        $candidates = [Collections.Generic.List[EdgePlus.Re.PlacedProbe]]::new()
        $candidates.Add([EdgePlus.Re.PlacedProbe]@{ Offset = 0; Located = $first })
        $second = Find-SecondWindow $Image $Scanner $Rva $level $first.Signature.Length
        if ($null -ne $second) {
            $candidates.Add($second)
        }
        foreach ($candidate in $candidates) {
            $located = $candidate.Located
            $key = '{0}|{1}|{2}|{3}' -f $candidate.Offset, $located.Signature.Text(), $located.Ordinal, $located.Total
            if ($seen.Add($key)) {
                $probes.Add($candidate)
            }
        }
    }
    return , [EdgePlus.Re.PlacedProbe[]]@($probes | Select-Object -First $script:ProbesPerEntry)
}

# How the code at Rva opens, masked: what a call into it must still land on.
function Get-TargetPattern {
    [OutputType([string])]
    param([EdgePlus.Re.PeImage] $Image, [uint32] $Rva)
    $code = $Image.Read($Rva, $script:TargetPatternLength + [EdgePlus.Re.SignatureDecoder]::InstructionSlack)
    $length = [EdgePlus.Re.SignatureDecoder]::OpeningLength($code, $script:TargetPatternLength)
    if ($length -eq 0) {
        return ''
    }
    $found = [EdgePlus.Re.SignatureDecoder]::MaskedBytes($code, [EdgePlus.Re.SignatureLevel]::RelocationAware, $length)
    if ($null -eq $found) {
        return ''
    }
    return $found.Text()
}

class Host {
    [uint32] $Start
    [int] $OperandOffset
    [int] $OperandExtra
    [object[]] $Probes      # EdgePlus.Re.PlacedProbe
    [uint32] $Size
}

# The code an entry stands on, fingerprinted whole: on another line of Edge a
# feature runs only while every entry's body is the code it was reversed from
# (signature_table.h's Body).
class Body {
    [uint64] $Hash
    [int[]] $MaskOffsets
    [uint32] $Size
    [uint32] $Offset        # from the body's start to the entry's own anchor
    [bool] $IsListed        # the body is a function of the exception table
}

# The fingerprint of the Size bytes at Start; $null when there are none.
function Get-Body {
    [OutputType([Body])]
    param(
        [EdgePlus.Re.PeImage] $Image,
        [uint32] $Start,
        [uint32] $Size,
        [uint32] $Offset,
        [bool] $IsListed
    )
    if ($Size -eq 0) {
        return $null
    }
    $code = $Image.Read($Start, [int]$Size)
    $mask = [EdgePlus.Re.SignatureDecoder]::BodyMaskOffsets($code)
    return [Body]@{
        Hash        = [EdgePlus.Re.SignatureDecoder]::BodyHash($code, $mask)
        MaskOffsets = [int[]]$mask.ToArray()
        Size        = $Size
        Offset      = $Offset
        IsListed    = $IsListed
    }
}

# The body of the exception-table function holding Rva, $Offset from its start.
function Get-ContainingBody {
    [OutputType([Body])]
    param([EdgePlus.Re.PeImage] $Image, [uint32] $Rva)
    $start = $Image.FunctionStart($Rva)
    if ($null -eq $start) {
        return $null
    }
    return Get-Body $Image $start ($Image.FunctionEnd($Rva) - $start) ($Rva - $start) $true
}

# A function that refers to the data at Rva and has a signature, or $null.
function Find-DataHost {
    [OutputType([Host])]
    param(
        [EdgePlus.Re.PeImage] $Image,
        [EdgePlus.Re.SignatureScanner] $Scanner,
        [System.Collections.Generic.Dictionary[uint32, System.Collections.Generic.List[EdgePlus.Re.RipReference]]] $References,
        [uint32] $Rva
    )
    $list = $null
    if (-not $References.TryGetValue($Rva, [ref]$list)) {
        return $null
    }
    $sorted = [Collections.Generic.List[EdgePlus.Re.RipReference]]::new($list)
    $sorted.Sort()
    foreach ($reference in $sorted) {
        $functionStart = $Image.FunctionStart($reference.DisplacementRva)
        if ($null -eq $functionStart) {
            continue
        }
        $probes = Get-Probes $Image $Scanner $functionStart $script:DefaultMaximumLength
        if ($probes.Count -gt 0) {
            return [Host]@{
                Start         = $functionStart
                OperandOffset = $reference.DisplacementRva - $functionStart
                OperandExtra  = $reference.Extra
                Probes        = $probes
                Size          = $Image.FunctionEnd($reference.DisplacementRva) - $functionStart
            }
        }
    }
    return $null
}

# A function that calls Target and has a signature, or $null: the caller found by
# signature, the call in it and the call's target stand in for a signature of the
# target itself.
function Find-CalleeHost {
    [OutputType([Host])]
    param(
        [EdgePlus.Re.PeImage] $Image,
        [EdgePlus.Re.SignatureScanner] $Scanner,
        [System.Collections.Generic.Dictionary[uint32, System.Collections.Generic.List[uint32]]] $Callers,
        [uint32] $Target
    )
    $list = $null
    if (-not $Callers.TryGetValue($Target, [ref]$list)) {
        return $null
    }
    $sorted = [Collections.Generic.List[uint32]]::new($list)
    $sorted.Sort()
    foreach ($displacementRva in $sorted) {
        $hostStart = $Image.FunctionStart($displacementRva)
        $isOtherFunction = $null -ne $hostStart -and $hostStart -ne $Target
        if (-not $isOtherFunction) {
            continue
        }
        $probes = Get-Probes $Image $Scanner $hostStart $script:DefaultMaximumLength
        if ($probes.Count -gt 0) {
            return [Host]@{
                Start         = $hostStart
                OperandOffset = $displacementRva - $hostStart
                OperandExtra  = 0
                Probes        = $probes
                Size          = $Image.FunctionEnd($hostStart) - $hostStart
            }
        }
    }
    return $null
}

function Format-Probe {
    [OutputType([string])]
    param([EdgePlus.Re.PlacedProbe] $Probe)
    if ($null -eq $Probe) {
        return '{"", 0, 0, 0}'
    }
    $located = $Probe.Located
    return '{{"{0}", {1}, {2}, {3}}}' -f $located.Signature.Text(), $Probe.Offset, $located.Ordinal, $located.Total
}

# One row of kEntries, in the field order of signature_table.h's Entry.
function Format-Entry {
    [OutputType([string])]
    param(
        [uint32] $Rva,
        [string] $Kind,
        [int] $Features,
        [uint32] $Size,
        [int] $OperandOffset,
        [int] $OperandExtra,
        [object[]] $Probes,
        [string] $Name,
        [uint32] $TargetSize = 0,
        [string] $TargetPattern = '',
        [int] $DataDelta = 0,
        [Body] $Body = $null,
        [string] $BodyMaskName = ''
    )
    $rendered = [Collections.Generic.List[string]]::new()
    foreach ($probe in @($Probes)) {
        if ($null -ne $probe) {
            $rendered.Add((Format-Probe $probe))
        }
    }
    while ($rendered.Count -lt $script:ProbesPerEntry) {
        $rendered.Add((Format-Probe $null))
    }
    return '    {{0x{0:x}, "{1}", "{2}", {{{{{3}}}}}, {4}, {5}, {6}, {7}, {8}, 0x{9:x2}, EntryKind::{10}, {11}}},' -f
        $Rva, $TargetPattern, $Name, ($rendered -join ', '), (Format-Body $Body $BodyMaskName), $Size,
        $OperandOffset, $TargetSize, $DataDelta, $Features, $Kind, $OperandExtra
}

# A row's Body: {hash, mask, mask count, size, offset, is listed}.
function Format-Body {
    [OutputType([string])]
    param([Body] $Body, [string] $MaskName)
    if ($null -eq $Body) {
        return '{0, nullptr, 0, 0, 0, false}'
    }
    $hasMask = $Body.MaskOffsets.Count -gt 0
    $mask = if ($hasMask) { $MaskName } else { 'nullptr' }
    $listed = if ($Body.IsListed) { 'true' } else { 'false' }
    return '{{0x{0:x16}ULL, {1}, {2}, {3}, {4}, {5}}}' -f $Body.Hash, $mask, $Body.MaskOffsets.Count, $Body.Size,
        $Body.Offset, $listed
}

# The array a row's Body points its mask at.
function Format-BodyMask {
    [OutputType([string])]
    param([Body] $Body, [string] $MaskName)
    $hasMask = $null -ne $Body -and $Body.MaskOffsets.Count -gt 0
    if (-not $hasMask) {
        return $null
    }
    return 'inline constexpr uint32_t {0}[] = {{{1}}};' -f $MaskName, ($Body.MaskOffsets -join ', ')
}

# Whether the table lists exactly the entries (name and value) offsets.h has: a
# table older than offsets.h would leave an address nothing can place.
function Test-SignatureTable {
    [OutputType([bool])]
    param([string] $Offsets, [string] $Table)
    $wanted = [Collections.Generic.HashSet[string]]::new()
    foreach ($entry in Get-OffsetEntries (Read-SourceText $Offsets)) {
        [void]$wanted.Add(('{0} 0x{1:x}' -f $entry.Name, $entry.Rva))
    }
    $found = [Collections.Generic.HashSet[string]]::new()
    foreach ($match in [regex]::Matches((Read-SourceText $Table), $script:TableEntryPattern)) {
        [void]$found.Add(('{0} 0x{1:x}' -f $match.Groups[2].Value, [Convert]::ToUInt32($match.Groups[1].Value, 16)))
    }
    $onlyInTable = [Collections.Generic.HashSet[string]]::new($found)
    $onlyInTable.ExceptWith($wanted)
    $onlyInOffsets = [Collections.Generic.HashSet[string]]::new($wanted)
    $onlyInOffsets.ExceptWith($found)
    [string[]] $stale = @($onlyInTable | Sort-Object) + @($onlyInOffsets | Sort-Object)
    foreach ($line in $stale | Select-Object -First 20) {
        Write-Host "[signatures] $line differs between $Offsets and $Table"
    }
    return $stale.Count -eq 0
}

Export-ModuleMember -Function Initialize-Signatures, Read-SourceText, Get-FeatureBits, Get-OffsetEntries,
    Get-EntryFeatures, Get-EntryKind, Find-Signature, Find-SecondWindow, Get-Probes, Get-TargetPattern,
    Find-DataHost, Find-CalleeHost, Format-Entry, Test-SignatureTable, Get-Body, Get-ContainingBody,
    Format-BodyMask, Get-UsageFeatures
Export-ModuleMember -Variable NeighbourReach, NeighbourStep, DefaultMaximumLength, SiteMaximumLength
