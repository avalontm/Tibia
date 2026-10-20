param(
    [string]$Executable = (Join-Path $PSScriptRoot '..\Tibia.exe'),
    [string]$Output = (Join-Path $PSScriptRoot '..\docs\protocol-dispatch-routes.csv')
)

$ErrorActionPreference = 'Stop'
$expectedSha256 = 'fe1172d0ff2e3024b78125184a4ebcdbb0489b9629ff744184ca130033d6321a'
$actualSha256 = (Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash.ToLowerInvariant()
if ($actualSha256 -ne $expectedSha256) {
    throw "Unexpected Tibia.exe SHA-256: $actualSha256"
}

$bytes = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $Executable).Path)
function Read-U16([byte[]]$Data, [int]$Offset) {
    [BitConverter]::ToUInt16($Data, $Offset)
}
function Read-U32([byte[]]$Data, [int]$Offset) {
    [BitConverter]::ToUInt32($Data, $Offset)
}

$peOffset = [int](Read-U32 $bytes 0x3C)
if ([Text.Encoding]::ASCII.GetString($bytes, $peOffset, 4) -ne "PE`0`0") {
    throw 'Input is not a valid PE image.'
}
$coffOffset = $peOffset + 4
if ((Read-U16 $bytes $coffOffset) -ne 0x14C) {
    throw 'Expected the analyzed x86 PE executable.'
}
$sectionCount = Read-U16 $bytes ($coffOffset + 2)
$optionalSize = Read-U16 $bytes ($coffOffset + 16)
$optionalOffset = $coffOffset + 20
if ((Read-U16 $bytes $optionalOffset) -ne 0x10B) {
    throw 'Expected a PE32 optional header.'
}
$imageBase = Read-U32 $bytes ($optionalOffset + 28)
$sectionOffset = $optionalOffset + $optionalSize
$sections = for ($i = 0; $i -lt $sectionCount; $i++) {
    $offset = $sectionOffset + 40 * $i
    [pscustomobject]@{
        Name = [Text.Encoding]::ASCII.GetString($bytes, $offset, 8).Trim([char]0)
        VirtualSize = Read-U32 $bytes ($offset + 8)
        VirtualAddress = Read-U32 $bytes ($offset + 12)
        RawSize = Read-U32 $bytes ($offset + 16)
        RawPointer = Read-U32 $bytes ($offset + 20)
    }
}

function Convert-VaToFileOffset([uint32]$Address) {
    if ($Address -lt $imageBase) { throw ('Address below image base: 0x{0:X8}' -f $Address) }
    $rva = [uint64]$Address - [uint64]$imageBase
    foreach ($section in $sections) {
        $extent = [Math]::Max([uint64]$section.VirtualSize, [uint64]$section.RawSize)
        if ($rva -ge $section.VirtualAddress -and $rva -lt ([uint64]$section.VirtualAddress + $extent)) {
            $delta = $rva - [uint64]$section.VirtualAddress
            if ($delta -ge $section.RawSize) { throw ('Address has no file bytes: 0x{0:X8}' -f $Address) }
            return [int]([uint64]$section.RawPointer + $delta)
        }
    }
    throw ('Address is outside PE sections: 0x{0:X8}' -f $Address)
}

$tables = @(
    @{ Phase = 'state4'; FirstOpcode = 0x0A; Count = 0x5B; MapVa = 0x0045004C; JumpVa = 0x00450028; Fallback = 0x0044F8E4 },
    @{ Phase = 'state6'; FirstOpcode = 0x0A; Count = 0xE8; MapVa = 0x004501AC; JumpVa = 0x004500A8; Fallback = 0x0044FE32 }
)
$records = [Collections.Generic.List[string]]::new()
$records.Add('phase,opcode,dispatch_case_address')
foreach ($table in $tables) {
    $mapOffset = Convert-VaToFileOffset ([uint32]$table.MapVa)
    $jumpOffset = Convert-VaToFileOffset ([uint32]$table.JumpVa)
    $recognized = 0
    for ($i = 0; $i -lt $table.Count; $i++) {
        $slot = [int]$bytes[$mapOffset + $i]
        $caseAddress = Read-U32 $bytes ($jumpOffset + 4 * $slot)
        if ($caseAddress -ne [uint32]$table.Fallback) {
            $opcode = [int]$table.FirstOpcode + $i
            $records.Add(('{0},0x{1:X2},0x{2:X8}' -f $table.Phase, $opcode, $caseAddress))
            $recognized++
        }
    }
    Write-Output ('{0}: {1} routed opcode values' -f $table.Phase, $recognized)
}

$outputPath = [IO.Path]::GetFullPath($Output)
$outputDirectory = [IO.Path]::GetDirectoryName($outputPath)
[IO.Directory]::CreateDirectory($outputDirectory) | Out-Null
[IO.File]::WriteAllLines($outputPath, $records, [Text.UTF8Encoding]::new($false))
Write-Output ('Wrote {0}' -f $outputPath)
