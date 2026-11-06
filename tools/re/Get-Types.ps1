<#
.SYNOPSIS
  Dump exact struct layouts (member offset, size, type) from the public PDB.
.DESCRIPTION
  Reads the same cache as Get-Symbols.ps1 through dbghelp's type information.
.PARAMETER Source
  The cache folder, or the path of msedge.dll.pdb (which needs no image).
.PARAMETER Name
  The types to dump.
.EXAMPLE
  pwsh tools\re\Get-Types.ps1 $cache 'views::ProposedLayout' 'gfx::Rect'
#>
[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory, Position = 0)]
    [string] $Source,
    [Parameter(Mandatory, Position = 1, ValueFromRemainingArguments)]
    [string[]] $Name
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Re.psm1') -Force

$isPdb = $Source.EndsWith('.pdb', [StringComparison]::OrdinalIgnoreCase)
$symbols = if ($isPdb) { Open-Symbols $Source } else { Open-Symbols (Get-EdgeDll) $Source }
try {
    foreach ($type in $Name) {
        "`n== $type =="
        $length = $null
        $members = $symbols.Members($type, [ref]$length)
        if ($null -eq $members) {
            '  (type not found)'
            continue
        }
        if ($null -ne $length) {
            '  sizeof = {0} (0x{0:x})' -f $length
        }
        foreach ($member in $members) {
            '  +0x{0,-4:x} {1,4}  {2} : {3}' -f $member.Offset, $member.Size, $member.Name, $member.TypeName
        }
    }
} finally {
    $symbols.Dispose()
}
