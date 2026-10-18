# msedge.dll reverse-engineering helpers

Target: the installed `C:\Program Files (x86)\Microsoft\Edge\Application\<version>\msedge.dll`
(the highest version; `MSEDGE_DLL` overrides it; image base `0x180000000`). Built `-fno-rtti`, stripped — but
Microsoft serves a **public PDB** for it, which is the whole game.

## One-time: fetch the PDB into a symbol cache

The CodeView GUID+age comes from `python tools/re/pdbid.py` (for 155.0.4283.62 it is
`29CB6181921D73B44C4C44205044422E1`; the `.45` build's was `EE0411C70C6A36F64C4C44205044422E1`).

```bash
CACHE="<somewhere>/symbols/msedge.dll.pdb/<GUID+age>"
mkdir -p "$CACHE"
curl -sL -A "Microsoft-Symbol-Server/10.0" -o "$CACHE/msedge.dll.pdb" \
  "https://msdl.microsoft.com/download/symbols/msedge.dll.pdb/<GUID+age>/msedge.dll.pdb"
```

(If Edge updates, re-read the GUID with `pdbid.py` and refetch; every finding in
`docs/re` is pinned to the build it names.)

## Enumerate symbols

```bash
python tools/re/symbols.py "$CACHE" "*SplitTab*" "*MultiContents*"
# -> "<rva>\t<undecorated name>"  (dbghelp, native; no third-party dep)
```

## Disassemble a named function, resolving call/lea targets to symbols

```bash
python tools/re/disasm.py "$CACHE" "MultiContentsView::GetViewSizes" "..."
# needs: pip install capstone  (5.x)
```

## After an Edge update: port `offsets.h`

Run this **before** the old Edge is gone if you can, but it needs only the two PDBs
(the old build's PDB stays on the symbol server). Fetch both PDBs as above, then:

```bash
python tools/re/port_offsets.py --old-pdb <old>/msedge.dll.pdb --new-pdb <new>/msedge.dll.pdb     --new-dll "<...>\<new version>\msedge.dll" --table docs/re/port-<build>.md
```

It dumps each PDB once (`dump_symbols.py`, cached as `<pdb>.tsv`), then for every
RVA of `offsets.h` finds the symbol(s) at the old RVA, looks the name up in the new
PDB, and reports `high` (unique name, equal size), `medium`/`low` (resolved by rank,
a changed size, or a patch site inside a function) or UNRESOLVED. `--old-dll`, when
the old image is still there, adds a check that the first instructions of each
function match. Re-run with `--apply` (and `--override NAME=0xRVA:why` for what the
PDB cannot place, e.g. a patch site in symbol-less cold code) to rewrite
`offsets.h`; then clang-format it, bump `kVersionRevision` (the panel reads its
pinned build from the same header), and fix the old RVAs quoted in comments (the
tool lists them).

What the PDB cannot port, and so must be checked by hand each time:

- **String resource ids** (`COPILOT_*`, `ControlButtons_more_string_id`): they shift
  with the resource table. Find each at its call site in the new image
  (`tools/re/disasm.py`, or `l10n` calls with `mov edx/r8d, <id>`).
- **Patch sites** in code with no symbol; `verify.h` refuses the whole DLL when
  one does not hold the instruction it was reversed from.
- **Struct offsets**: the public PDB has no types, so none can be diffed. Equal
  function sizes are the evidence; then run the live test in `docs/re/port-4283-62.md`.
