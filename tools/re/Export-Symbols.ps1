<#
.SYNOPSIS
  Dump every symbol of an msedge.dll public PDB as "rva<TAB>size<TAB>decorated name".
.DESCRIPTION
  Needs only the PDB, not the image, so an old build can be dumped after Edge has
  replaced its dll. Port-Offsets.ps1 diffs two such dumps.
.PARAMETER Pdb
  The msedge.dll.pdb to read.
.PARAMETER Out
  The tab-separated file to write.
#>
param(
    [Parameter(Mandatory)]
    [string] $Pdb,
    [Parameter(Mandatory)]
    [string] $Out
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Re.psm1') -Force

$symbols = Open-Symbols (Resolve-Path $Pdb).Path -Decorated
try {
    $rows = $symbols.Enumerate('*') | ForEach-Object { '{0:x}{1}{2}{1}{3}' -f $_.Rva, "`t", $_.Size, $_.Name }
    [IO.File]::WriteAllLines($Out, [string[]]$rows)
    [Console]::Error.WriteLine("$(@($rows).Count) symbols")
} finally {
    $symbols.Dispose()
}
