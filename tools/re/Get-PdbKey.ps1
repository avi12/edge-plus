<#
.SYNOPSIS
  Print the PDB name and symbol-server key of a PE, from its debug directory.
.PARAMETER Dll
  The PE to read; the installed msedge.dll by default.
#>
param(
    [string] $Dll
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Re.psm1') -Force

if (-not $Dll) {
    $Dll = Get-EdgeDll
}
$key = Get-PdbKey $Dll
"$($key.Name) $($key.Key)"
$key.Url
