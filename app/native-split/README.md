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
- [x] N-pane geometry — `CalculateProposedLayout` tiles a directional split tree
- [x] N-pane insert — the `index < 2` guard lifted; containers grow with the split
- [x] Hot-keys (`Ctrl+Shift+Alt` + arrows / A / S / X): directional add, swap, collapse
- [x] Animated transitions — add / swap / collapse ease each pane into place
      (~220 ms); window resizes snap; off when Windows "Animation effects" is off
- [x] Drag-to-rearrange panes — a grip in each pane's hover bar; drop beside a
      pane, on the split's outer edge, or on a pane's centre to swap
- [x] Close a pane — its X closes just that pane; the rest reflow, animated
- [x] Dividers — drag between any two panes; proportions persist; double-click
      one to even out the split it divides
- [x] ⋯ "Add pane" — a d-pad on the pane bar's ⋯ adds a pane beside that pane
      (`docs/re/pane-menu.md`); it opens on Edge's tab offer, where picking a
      tab or the X acts on that pane alone (`docs/re/split-offer.md`)
- [x] Alt + drag a tab — hovering a split's tab shows that split; dropping on
      it docks the tab as a new pane (`docs/re/tab-drag.md`)
- [x] New-tab links in a split — the ⋯ toggle "Open left links to the right"
      off: a pane's `target=_blank` link opens in that pane; on: in a new pane
      right of it (`docs/re/split-links.md`)
- [x] Move between panes — `Shift+Alt` + an arrow key focuses the pane on that
      side (parity with the extension's pane focus)
- [x] Companion-app toggle: inject / unload / hot-swap from the panel; toggling
      off breaks every split of 3+ panes back into standalone tabs first

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
