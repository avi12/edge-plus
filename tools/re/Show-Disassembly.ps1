<#
.SYNOPSIS
  Disassemble named msedge.dll functions, resolving call and lea targets to PDB symbols.
.DESCRIPTION
  Answers questions such as "where is the split capped at two" by reading the
  geometry functions directly. The public PDB records no size for most functions;
  -Size sets how many bytes to read then (at most -Max when the PDB has one).
.PARAMETER Cache
  The folder holding msedge.dll.pdb.
.PARAMETER Name
  The functions to disassemble.
#>
[CmdletBinding(PositionalBinding = $false)]
param(
    [Parameter(Mandatory, Position = 0)]
    [string] $Cache,
    [Parameter(Mandatory, Position = 1, ValueFromRemainingArguments)]
    [string[]] $Name,
    [int] $Size = 400,
    [int] $Max = 500
)

$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'EdgePlus.Re.psm1') -Force

$dll = Get-EdgeDll
$image = Read-MappedImage $dll
$symbols = Open-Symbols $dll $Cache
try {
    foreach ($target in $Name) {
        $function = $symbols.FromName($target)
        if (-not $function) {
            "`n### ${target}: not found"
            continue
        }
        $length = if ($function.Size) { [Math]::Min($function.Size, $Max) } else { $Size }
        "`n### {0}  rva {1:x}  size {2}" -f $target, $function.Rva, $length
        foreach ($instruction in Get-Instructions $image $function.Rva $length) {
            $note = ''
            $isTargetShown = $null -ne $instruction.Target -and $instruction.Mnemonic -match '^(call|jmp|lea|j)'
            if ($isTargetShown) {
                $displacement = [uint64]0
                $owner = $symbols.FromRva($instruction.Target, [ref]$displacement)
                if ($owner) {
                    $suffix = if ($displacement) { '+0x{0:x}' -f $displacement } else { '' }
                    $note = " ; -> $($owner.Name)$suffix"
                }
            }
            '  {0:x6}: {1}{2}' -f $instruction.Rva, $instruction.Text, $note
        }
    }
} finally {
    $symbols.Dispose()
}
