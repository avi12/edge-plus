# Fix Edge

Live feature switches for Microsoft Edge. A WinUI 3 panel turns each feature on
or off, and Edge picks the change up the moment it is made - no restart, ever,
including when the app itself is updated.

## Features

- **Split into any grid** - split a tab into as many panes as you like, side by
  side or stacked, inside the tab itself; each tab keeps its own layout, the
  way Arc does it. Every pane has its own address bar, and dividers drag. From
  the grid button in Edge's toolbar (presets, split right/down, merge), the
  pane bars, or a right-click on a page or link.
- **Tabs into panes, as many as you like** - Ctrl + click tabs in the tab strip,
  then "Put the selected tabs in panes" in the toolbar popup, the way Arc makes
  a split from a selection. Include a split tab and the rest join it. New panes
  fill out a grid rather than one ever-thinner row, and "Even out into a grid"
  re-tiles any number of panes.
- **Move between panes** - Alt + Shift + an arrow key jumps to the pane on that
  side.
- **Hide the developer-mode prompt** - stops Edge asking to turn off
  developer-mode extensions at startup.

Why the panes are frames in a page rather than an extended Edge split view:
[docs/edge-split-view.md](docs/edge-split-view.md).

## How it fits together

```
app/
  panel/      the WinUI 3 panel (fixedge.exe): one switch per feature
  host/       the native-messaging host (fixedge-host.exe, NativeAOT)
extension/    the Edge extension (WXT, Svelte 5, @webext-core/messaging, zod)
```

The panel writes `%LOCALAPPDATA%\FixEdge\settings.json`; the host, started by
Edge when the extension connects, watches it and relays every change; the
extension switches the feature modules on and off. The feature list itself is
`extension/src/features/catalog.json`, read by both sides.

The panel carries the extension and the host inside itself and unpacks them
into `%LOCALAPPDATA%\FixEdge`. A newer extension reloads itself in place; a
newer host is swapped in by renaming the running one aside.

## Setup

```powershell
pnpm --dir extension install
dotnet build app/panel -c Release      # builds the extension and the host too
app\panel\bin\Release\net10.0-windows10.0.26100.0\win-x64\fixedge.exe
```

Then once, in Edge: `edge://extensions` → Developer mode → Load unpacked →
`%LOCALAPPDATA%\FixEdge\extension` (the panel offers the button and the path).

Split and merge have no default keys - Edge allows four, and the arrows went to
moving between panes. Bind them at `edge://extensions/shortcuts`.

## Develop

In `extension/`:

```powershell
pnpm test           # layout and neighbour geometry, under node --test
pnpm svelte:check   # types
pnpm ext:dev        # WXT dev browser
```
