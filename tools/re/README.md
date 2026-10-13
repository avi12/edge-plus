# msedge.dll reverse-engineering helpers

Target: `C:\Program Files (x86)\Microsoft\Edge\Application\155.0.4283.45\msedge.dll`
(sha256 `ec287d2f…`, image base `0x180000000`). Built `-fno-rtti`, stripped — but
Microsoft serves a **public PDB** for it, which is the whole game.

## One-time: fetch the PDB into a symbol cache

The CodeView GUID+age for this build is `EE0411C70C6A36F64C4C44205044422E1`.

```bash
CACHE="<somewhere>/symbols/msedge.dll.pdb/EE0411C70C6A36F64C4C44205044422E1"
mkdir -p "$CACHE"
curl -sL -A "Microsoft-Symbol-Server/10.0" -o "$CACHE/msedge.dll.pdb" \
  "https://msdl.microsoft.com/download/symbols/msedge.dll.pdb/EE0411C70C6A36F64C4C44205044422E1/msedge.dll.pdb"
```

(If Edge updates, re-read the GUID: `python tools/re/pdbid.py` — or the debug-dir
parse in git history — then refetch; every finding below is pinned to this build.)

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
