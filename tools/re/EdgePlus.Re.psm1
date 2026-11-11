# What the tools/re scripts share: where the msedge.dll under analysis lives, its PDB
# key, the dbghelp wrapper (Dbghelp.cs) and the Iced disassembler.

Set-StrictMode -Version Latest

$script:ApplicationFolder = 'C:\Program Files (x86)\Microsoft\Edge\Application'
$script:IcedVersion = '1.21.0'
$script:IcedLibrary = 'lib/netstandard2.1/Iced.dll'
[uint64] $script:ImageBase = 0x180000000

class PdbKey {
    [string] $Name
    [string] $Key
    [string] $Url
}

class Instruction {
    [uint64] $Rva
    [int] $Length
    [string] $Mnemonic
    [string] $Text
    [Nullable[uint64]] $Target
}

# The single home of the DLL's path for tools/re. MSEDGE_DLL overrides it (to point at
# a copy of an older build); otherwise the highest installed version wins.
function Get-EdgeDll {
    [OutputType([string])]
    param()
    if ($env:MSEDGE_DLL) {
        return $env:MSEDGE_DLL
    }
    $candidates = Get-ChildItem (Join-Path $script:ApplicationFolder '*\msedge.dll') -ErrorAction SilentlyContinue |
        Where-Object { $_.Directory.Name -match '^\d+(\.\d+)+$' }
    if (-not $candidates) {
        throw "no msedge.dll under $script:ApplicationFolder"
    }
    return ($candidates | Sort-Object { [version]$_.Directory.Name } -Descending | Select-Object -First 1).FullName
}

# The PDB name and symbol-server key of a PE, from its CodeView debug entry.
function Get-PdbKey {
    [OutputType([PdbKey])]
    param([string] $Dll)
    $stream = [IO.File]::OpenRead($Dll)
    try {
        $reader = [System.Reflection.PortableExecutable.PEReader]::new($stream)
        $entry = $reader.ReadDebugDirectory() |
            Where-Object { $_.Type -eq [System.Reflection.PortableExecutable.DebugDirectoryEntryType]::CodeView } |
            Select-Object -First 1
        if (-not $entry) {
            throw "$Dll has no CodeView debug entry."
        }
        $data = $reader.ReadCodeViewDebugDirectoryData($entry)
        $name = Split-Path $data.Path -Leaf
        $key = '{0}{1:X}' -f $data.Guid.ToString('N').ToUpperInvariant(), $data.Age
        return [PdbKey]@{
            Name = $name
            Key  = $key
            Url  = "https://msdl.microsoft.com/download/symbols/$name/$key/$name"
        }
    } finally {
        $stream.Dispose()
    }
}

function Initialize-Dbghelp {
    [OutputType([void])]
    param()
    if (-not ('EdgePlus.Re.Dbghelp' -as [type])) {
        Add-Type -Path (Join-Path $PSScriptRoot 'Dbghelp.cs'), (Join-Path $PSScriptRoot 'SymbolTable.cs')
    }
}

# dbghelp over a PDB path (no image needed) or over the installed image plus a cache
# folder. Raw names when -Decorated.
function Open-Symbols {
    [OutputType('EdgePlus.Re.Dbghelp')]
    param([string] $Source, [string] $Cache = '.', [switch] $Decorated)
    Initialize-Dbghelp
    return [EdgePlus.Re.Dbghelp]::new($Source, $Cache, [bool]$Decorated)
}

# The decoder library, fetched once from NuGet into out\re (git-ignored).
function Initialize-Iced {
    [OutputType([void])]
    param()
    if ('Iced.Intel.Decoder' -as [type]) {
        return
    }
    $folder = Join-Path $PSScriptRoot "..\..\out\re\iced-$script:IcedVersion"
    $library = Join-Path $folder 'Iced.dll'
    if (-not (Test-Path $library)) {
        New-Item -ItemType Directory -Force $folder | Out-Null
        $package = Join-Path $folder 'iced.zip'
        Invoke-WebRequest "https://www.nuget.org/api/v2/package/Iced/$script:IcedVersion" -OutFile $package
        Add-Type -AssemblyName System.IO.Compression.FileSystem
        $archive = [IO.Compression.ZipFile]::OpenRead($package)
        try {
            $entry = $archive.GetEntry($script:IcedLibrary)
            [IO.Compression.ZipFileExtensions]::ExtractToFile($entry, $library, $true)
        } finally {
            $archive.Dispose()
        }
        Remove-Item $package
    }
    Add-Type -Path $library
}

# A PE mapped as the loader lays it out, so an RVA indexes it directly.
function Read-MappedImage {
    [OutputType([byte[]])]
    param([string] $Dll)
    $file = [IO.File]::ReadAllBytes($Dll)
    $stream = [IO.MemoryStream]::new($file)
    $reader = [System.Reflection.PortableExecutable.PEReader]::new($stream)
    $mapped = [byte[]]::new($reader.PEHeaders.PEHeader.SizeOfImage)
    [Array]::Copy($file, $mapped, $reader.PEHeaders.PEHeader.SizeOfHeaders)
    foreach ($section in $reader.PEHeaders.SectionHeaders) {
        $length = [Math]::Min($section.SizeOfRawData, $section.VirtualSize)
        [Array]::Copy($file, $section.PointerToRawData, $mapped, $section.VirtualAddress, $length)
    }
    $reader.Dispose()
    return , $mapped
}

# The instructions decoded from `Length` bytes at an RVA of a mapped image.
function Get-Instructions {
    [OutputType([Instruction])]
    param([byte[]] $Image, [uint64] $Rva, [int] $Length)
    Initialize-Iced
    $code = [byte[]]::new([Math]::Min($Length, $Image.Length - $Rva))
    [Array]::Copy($Image, [int64]$Rva, $code, 0, $code.Length)
    $decoder = [Iced.Intel.Decoder]::Create(64, [Iced.Intel.ByteArrayCodeReader]::new($code), $script:ImageBase + $Rva)
    $formatter = [Iced.Intel.IntelFormatter]::new()
    $output = [Iced.Intel.StringOutput]::new()
    $instruction = [Iced.Intel.Instruction]::new()
    while ($decoder.IP -lt $script:ImageBase + $Rva + $code.Length) {
        $decoder.Decode([ref]$instruction)
        if ($instruction.IsInvalid) {
            break
        }
        $formatter.Format([ref]$instruction, $output)
        [Nullable[uint64]] $target = $null
        if ($instruction.IsIPRelativeMemoryOperand) {
            $target = $instruction.IPRelativeMemoryAddress - $script:ImageBase
        } elseif ($instruction.Op0Kind -eq [Iced.Intel.OpKind]::NearBranch64) {
            $target = $instruction.NearBranchTarget - $script:ImageBase
        }
        [Instruction]@{
            Rva      = $instruction.IP - $script:ImageBase
            Length   = $instruction.Length
            Mnemonic = $instruction.Mnemonic.ToString().ToLowerInvariant()
            Text     = $output.ToStringAndReset()
            Target   = $target
        }
    }
}

Export-ModuleMember -Function Get-EdgeDll, Get-PdbKey, Initialize-Dbghelp, Open-Symbols, Initialize-Iced, Read-MappedImage, Get-Instructions
