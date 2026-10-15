# Fix Edge

Live feature switches for Microsoft Edge. A WinUI 3 panel turns each feature on
or off, and Edge picks the change up the moment it is made — no restart, ever.

## Features

- **Split into any grid** — grows Edge's *own* split view past two panes, side by
  side or stacked, inside the tab itself; each tab keeps its own layout, the way
  Arc does it. Every pane has its own address bar, dividers drag, and a pane's ⋯
  menu adds a pane above / below / left / right. Loaded straight into Edge as a
  DLL — no extension (`app/native-split`, reverse-engineered in `docs/re/`).
- **Move between panes** — `Shift + Alt` + an arrow key focuses the pane on that
  side, in the split you are on.
- **Hide the developer-mode prompt** — stops Edge asking to turn off
  developer-mode extensions at startup.

## How it fits together

```
app/
  panel/         the WinUI 3 panel (fixedge.exe): one switch per feature
  native-split/  the C++ DLL the panel injects into Edge's browser process
```

The panel writes `%LOCALAPPDATA%\FixEdge\settings.json` and drives each feature
directly: the split switch injects (or ejects) `fixedge-split.dll` into Edge's
browser process live, and hiding the developer-mode prompt is handled by the
panel itself. The split DLL travels inside the panel exe and is unpacked beside
the settings, so the DLL always matches the build.

Turning the split off breaks any split of three or more panes back into
standalone tabs first, so nothing Edge can't render on its own is left behind.

## Setup

```powershell
dotnet build app/panel -c Release      # builds and carries the split DLL too
app\panel\bin\Release\net10.0-windows10.0.26100.0\win-x64\fixedge.exe
```

Open the panel, open Edge, and flip the switches. There is no extension to load
and nothing to pre-install.

The split DLL is pinned to one `msedge.dll` build (`app/native-split/src/offsets.h`);
on any other build it refuses to hook and the split simply does nothing, so it is
always safe to leave on.

## Develop

The native split DLL:

```powershell
app\native-split\build.cmd       # -> build\fixedge-split.dll and inject.exe
app\native-split\test\run.cmd    # the pure-logic unit tests
pwsh tools\lint.ps1              # clang-format/-tidy + dotnet format
```
