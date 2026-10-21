# Edge+

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
- **Settings and extensions pages follow the theme** — `edge://settings` and
  `edge://extensions` take their light or dark from Windows or Edge's
  Appearance setting, never from the theme, so a dark theme on a light Windows
  leaves them white. With this on they follow the theme's own colors, live
  (including Theming's swap); a default-theme profile still follows Edge's
  Appearance setting. Only those two pages change; websites and every other
  Edge page keep what Edge decides (`docs/re/webui-color-scheme.md`). Runs in
  the injected DLL.
- **Hide the developer-mode prompt** — stops Edge asking to turn off
  developer-mode extensions at startup.
- **Ask to handle links, like Chrome** — when a site such as Gmail offers to
  open email or calendar links, Edge's address-bar handler icon stays an open
  question: closing its bubble or answering "Just this time" no longer blocks
  the site for good, and turning it on lifts the blocks already on record
  (Edge has no page for them) (`docs/re/protocol-handlers.md`). Runs in the
  injected DLL.
- **Enhanced find: RegExp and Ask Copilot** — Edge's own `Ctrl + F`
  bar gets two modes, chosen by what you type in its box: `/pattern/i` is a
  RegExp search (every match highlighted, counted in the bar's own n/m, Enter
  steps), and `? where do they discuss pricing?` asks Copilot and
  highlights the paragraphs it names. Anything else
  is Edge's own find, as are Edge pages and PDFs (`docs/re/find-bar.md`). Runs in
  the injected DLL.

Removed for now: the Gemini integration (Gemini in a side panel from the
toolbar chat button, with tab sharing). Copilot is the only assistant. The code
is in git history at `4ee5113`; the notes are in
`docs/re/removed-gemini-integration.md`.

## How it fits together

```
app/
  panel/         the Tauri panel (Rust core + Svelte UI): one switch per feature
    src/           the Svelte front end, styled as Fluent 2 by hand (no UI deps)
    src-tauri/     the Rust core; edge-native/ holds all the Win32 glue
  panel-winui/   the same panel as WinUI 3 / C# (modelled on avi/any-stt), on
                 its way to replacing the Tauri one; Native/ holds its Win32
  native-split/  the C++ DLL the panel injects into Edge's browser process
```

Both panels read and write the same `settings.json` and drive the DLL the same
way, so either can run.

The panel keeps the user's choices in `%LOCALAPPDATA%\FixEdge\settings.json` and
drives each feature directly. Every Win32 action — enumerating Edge's browser
processes, injecting (or ejecting) `fixedge-split.dll`, adding a theme through
Edge's external-extension loader, snoozing the developer-mode prompt — lives in
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
update's restart shows up. On any other build it switches the split, the
theming, the link-handler, the find and the WebUI-theme switches off (all call into `msedge.dll`; hiding the developer-mode prompt only
edits `Preferences` and stays on), says so, and never injects into that Edge.

## Develop

The panel (Rust + Svelte):

```powershell
pnpm --dir app/panel app          # live dev (Vite + Tauri)
pnpm --dir app/panel lint         # oxlint + eslint + stylelint + clippy + fallow
pnpm --dir app/panel check        # svelte-check
```

The WinUI panel (C#; .NET 10 SDK, Windows App SDK 1.8):

```powershell
dotnet build app\panel-winui                 # -> bin\...\edgeplus.exe (builds the DLL if missing)
dotnet publish app\panel-winui -c Release    # one self-contained edgeplus.exe
app\panel-winui\test\run.cmd                 # the pure-logic unit tests
pwsh tools\lint.ps1                          # includes dotnet format + the C# house rules
```

The native split DLL:

```powershell
app\native-split\build.cmd       # -> build\fixedge-split.dll and inject.exe
app\native-split\test\run.cmd    # the pure-logic unit tests
pwsh tools\lint.ps1              # clang-format/-tidy for the C++
```
