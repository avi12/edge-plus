<#
.SYNOPSIS
  Enumerate msedge.dll symbols from the downloaded public PDB (Windows dbghelp).
.DESCRIPTION
  Prints "RVA<TAB>undecorated name" for the symbols matching each mask.
.PARAMETER Cache
  The folder holding msedge.dll.pdb.
.PARAMETER Mask
  dbghelp masks; the split-view classes by default.
.EXAMPLE
  pwsh tools\re\Get-Symbols.ps1 $cache '*SplitTab*' '*MultiContents*'
#>
param(
    [Parameter(Mandatory)]
    [string] $Cache,
    [string[]] $Mask = @('*SplitTab*', '*MultiContents*', '*SplitWindow*', '*split_window*')
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Re.psm1') -Force

$symbols = Open-Symbols (Get-EdgeDll) $Cache
try {
    $rows = foreach ($pattern in $Mask) {
        $symbols.Enumerate($pattern)
    }
    $rows | Sort-Object Rva, Name -Unique | ForEach-Object { '0x{0:x8}{1}{2}' -f $_.Rva, "`t", $_.Name }
    [Console]::Error.WriteLine("$(@($rows | Sort-Object Rva, Name -Unique).Count) symbols")
} finally {
    $symbols.Dispose()
}
