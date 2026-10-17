# Fix Edge

Live feature switches for Microsoft Edge. A desktop panel turns each feature on
or off, and Edge picks the change up the moment it is made — no restart, ever.

## Features

- **Split into any grid** — grows Edge's *own* split view past two panes, side by
  side or stacked, inside the tab itself; each tab keeps its own layout, the way
  Arc does it. Every pane has its own address bar, dividers drag, and a pane's ⋯
  menu adds a pane above / below / left / right. Loaded straight into Edge as a
  DLL — no extension (`app/native-split`, reverse-engineered in `docs/re/`).
- **Move between panes** — `Shift + Alt` + an arrow key focuses the pane on that
  side, in the split you are on.
- **Theme by Windows light and dark** — use one theme when Windows is in light
  mode and another when it is in dark, switched live when Windows flips, with no
  restart. Add themes from a Chrome Web Store / Edge Add-ons link or a local
  `.crx`; Edge's own theming engine paints them (the injected DLL only picks and
  applies the right one).
- **Hide the developer-mode prompt** — stops Edge asking to turn off
  developer-mode extensions at startup.
- **Open mail and calendar links in Google** — `mailto:` links open a Gmail
  draft and `webcal:` links Google Calendar, as Chrome does once Gmail is
  allowed to handle them; an earlier Block of either is lifted. Both this and
  the prompt switch edit each profile's `Preferences` once Edge has exited, so
  they take effect from Edge's next start.

## How it fits together

```
app/
  panel/         the Tauri panel (Rust core + Svelte UI): one switch per feature
    src/           the Svelte front end, styled as Fluent 2 by hand (no UI deps)
    src-tauri/     the Rust core; edge-native/ holds all the Win32 glue
  native-split/  the C++ DLL the panel injects into Edge's browser process
```

The panel keeps the user's choices in `%LOCALAPPDATA%\FixEdge\settings.json` and
drives each feature directly. Every Win32 action — enumerating Edge's browser
processes, injecting (or ejecting) `fixedge-split.dll`, adding a theme through
Edge's external-extension loader, waiting for Edge to exit to edit its
`Preferences` — lives in
the `edge-native` crate, so the rest of the Rust core keeps `unsafe` forbidden.
The split DLL travels inside the panel binary and is unpacked beside the
settings, so the DLL always matches the build.

The split and the theming share one injected DLL, driven by a small config file
(`fixedge-config.bin`) the DLL re-reads whenever a named event is signalled.
Turning the split off breaks any split of three or more panes back into
standalone tabs first, so nothing Edge can't render on its own is left behind.

## Setup

```powershell
pnpm --dir app/panel install
pnpm --dir app/panel app          # run the panel (tauri dev)
pnpm --dir app/panel app:build    # package it (tauri build)
```

Open the panel, open Edge, and flip the switches. There is no extension to load
and nothing to pre-install.

The split DLL is pinned to one `msedge.dll` build (`app/native-split/src/offsets.h`);
on any other build it refuses to hook and the split simply does nothing, so it is
always safe to leave on. The panel reads that build from `offsets.h` at compile
time and compares it with the `msedge.dll` each Edge browser process actually
loaded — at start and whenever a new browser process appears, which is how an
update's restart shows up. On any other build it switches the split and the
theming off (both call into `msedge.dll`; hiding the developer-mode prompt and
the Google link handlers only edit `Preferences` and stay on), says so, and never
injects into that Edge.

## Develop

The panel (Rust + Svelte):

```powershell
pnpm --dir app/panel app          # live dev (Vite + Tauri)
pnpm --dir app/panel lint         # oxlint + eslint + stylelint + clippy + fallow
pnpm --dir app/panel check        # svelte-check
```

The native split DLL:

```powershell
app\native-split\build.cmd       # -> build\fixedge-split.dll and inject.exe
app\native-split\test\run.cmd    # the pure-logic unit tests
pwsh tools\lint.ps1              # clang-format/-tidy for the C++
```
