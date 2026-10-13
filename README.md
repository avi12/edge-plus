# Fix Edge

Live feature switches for Microsoft Edge. A WinUI 3 panel turns each feature on
or off, and Edge picks the change up the moment it is made - no restart, ever,
including when the app itself is updated.

## Features

- **Split into any grid** - split a tab into as many panes as fit, side by side
  or stacked, each tab with its own layout, the way Arc does it. Switching tabs
  switches the panes. From the grid button in Edge's toolbar (presets, split
  right/down, merge) or a right-click on a page or link.
- **Move between panes** - Alt + Shift + an arrow key jumps to the pane on that
  side.

Why the panes are windows rather than an extended Edge split view:
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
