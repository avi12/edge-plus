# fixedge-split — native N-pane split for Edge

Grows Microsoft Edge's built-in split view past its two-pane limit by injecting a
DLL into the browser process and reshaping `MultiContentsView`'s geometry. This is
the "no extension, hot-swappable DLL" path (see `docs/re/split-view-abi.md` for the
reverse-engineered ABI, and `docs/edge-split-view.md` for why the two-pane cap
exists). Authorized personal customization of the user's own Edge.

## Status

- [x] **Reverse-engineering** — Microsoft's public PDB for `msedge.dll` gives full
      symbols; the split engine, its struct offsets and the exact two-pane cap
      sites are mapped (`docs/re/`).
- [x] **Feasibility proven** — `fixedge-split.dll` injects into Edge's browser
      process (no Code Integrity Guard there), locates `msedge.dll`, and MinHook
      installs on the verified RVAs; offsets resolve correctly live. The DLL reads
      the split's live pane count through the hooks.
- [ ] N-pane geometry (`CalculateProposedLayout`/`GetViewSizes`/separators)
- [ ] N-pane insert (drop the `index < 2` guard; grow containers + resize areas)
- [ ] Companion-app toggle: inject / unload / hot-swap from the panel

## Build

```powershell
.\build.cmd      # -> build\fixedge-split.dll and build\inject.exe (MSVC Build Tools)
```

## Test against your own Edge (never your main windows)

Launch an isolated instance, then inject:

```powershell
$prof = "$env:TEMP\fixedge-split-test"
Start-Process 'C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe' `
  "--user-data-dir=$prof","--new-window","https://example.com"
# find the browser pid (msedge.exe with that profile, no --type=), then:
.\build\inject.exe <browser-pid> "$PWD\build\fixedge-split.dll"
```

The DLL logs to `%LOCALAPPDATA%\FixEdge\split-native.log`.
