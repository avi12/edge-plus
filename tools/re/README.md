# msedge.dll reverse-engineering helpers

Target: the installed `C:\Program Files (x86)\Microsoft\Edge\Application\<version>\msedge.dll`
(the highest version; `MSEDGE_DLL` overrides it; image base `0x180000000`). Built `-fno-rtti`, stripped — but
Microsoft serves a **public PDB** for it, which is the whole game.

## One-time: fetch the PDB into a symbol cache

The CodeView GUID+age comes from `pwsh tools/re/Get-PdbKey.ps1` (for 155.0.4283.62 it is
`29CB6181921D73B44C4C44205044422E1`; the `.45` build's was `EE0411C70C6A36F64C4C44205044422E1`).

```bash
CACHE="<somewhere>/symbols/msedge.dll.pdb/<GUID+age>"
mkdir -p "$CACHE"
curl -sL -A "Microsoft-Symbol-Server/10.0" -o "$CACHE/msedge.dll.pdb" \
  "https://msdl.microsoft.com/download/symbols/msedge.dll.pdb/<GUID+age>/msedge.dll.pdb"
```

(If Edge updates, re-read the GUID with `Get-PdbKey.ps1` and refetch; every finding in
`docs/re` is pinned to the build it names.)

## Enumerate symbols

```bash
pwsh tools/re/Get-Symbols.ps1 "$CACHE" "*SplitTab*" "*MultiContents*"
# -> "<rva>\t<undecorated name>"  (dbghelp through tools/re/Dbghelp.cs)
```

## Disassemble a named function, resolving call/lea targets to symbols

```bash
pwsh tools/re/Show-Disassembly.ps1 "$CACHE" "MultiContentsView::GetViewSizes" "..."
# the Iced decoder is fetched from NuGet once into oute
```

## After an Edge update: port `offsets.h`

Run this **before** the old Edge is gone if you can, but it needs only the two PDBs
(the old build's PDB stays on the symbol server). Fetch both PDBs as above, then:

```bash
pwsh tools/re/Port-Offsets.ps1 -OldPdb <old>/msedge.dll.pdb -NewPdb <new>/msedge.dll.pdb     -NewDll "<...>\<new version>\msedge.dll" -Table docs/re/port-<build>.md
```

It dumps each PDB once (`Export-Symbols.ps1`, cached as `<pdb>.tsv`), then for every
RVA of `offsets.h` finds the symbol(s) at the old RVA, looks the name up in the new
PDB, and reports `high` (unique name, equal size), `medium`/`low` (resolved by rank,
a changed size, or a patch site inside a function) or UNRESOLVED. `-OldDll`, when
the old image is still there, adds a check that the first instructions of each
function match. Re-run with `-Apply` (and `-Override NAME=0xRVA:why` for what the
PDB cannot place, e.g. a patch site in symbol-less cold code) to rewrite
`offsets.h`; then clang-format it, bump `kVersionRevision` (the panel reads its
pinned build from the same header), and fix the old RVAs quoted in comments (the
tool lists them).

What the PDB cannot port, and so must be checked by hand each time:

- **String resource ids** (`COPILOT_*`, `ControlButtons_more_string_id`): they shift
  with the resource table. Find each at its call site in the new image
  (`tools/re/Show-Disassembly.ps1`, or `l10n` calls with `mov edx/r8d, <id>`).
- **Patch sites** in code with no symbol; `verify.h` refuses the whole DLL when
  one does not hold the instruction it was reversed from.
- **Struct offsets**: the public PDB has no types, so none can be diffed. Equal
  function sizes are the evidence; then run the live test in `docs/re/port-4283-62.md`.

## The signature table: finding `offsets.h` in another build

```bash
pwsh tools/re/New-SignatureTable.ps1          # -> app/dll/src/edge/signatures.g.h
pwsh tools/re/New-SignatureTable.ps1 -Check   # exit 1 when the table lists other entries than offsets.h
```

It signs the pinned build's `msedge.dll` (`kVersion*` of `offsets.h`; `-Dll`, `-Offsets`,
`-KeyOffsets`, `-Version`, `-Out` override) and writes one row per `rva` entry in the
shape of `app/dll/src/edge/signature_table.h`, whose `kFeatureBit*` it reads. A signature
is a function's opening bytes with only what a rebuild moves wildcarded (rel32 branch
targets and RIP-relative displacements; the tolerant mask also every displacement and
32/64-bit immediate), grown until nothing else in `.text` matches, never past the
function's end in `.pdata`. Each entry gets up to four probes (strict and tolerant, the
opening and a window further in) and is a `kFunction`, `kSite`, `kCallee` (found through
a caller's `call`), `kInside` (a fixed distance into a signed function) or `kData` (the
RIP displacement of a signed function that addresses it, or a neighbour `dataDelta`
away). Which banner or name of `offsets.h` belongs to which feature is
`EdgePlus.Signatures.psm1`'s; the byte work is `Signatures.cs`, on the Iced decoder.
About 15 s per run.

`Measure-Signatures.ps1 -Out report.json -Build 'a=<dll>,<offsets.h>', 'b=...'` measures
how many entries the signatures of each build find correctly in the others.
