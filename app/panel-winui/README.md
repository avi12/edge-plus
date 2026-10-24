# The panel

Edge+'s panel, WinUI 3 / C#, modelled on `avi/any-stt`'s app. It keeps the
user's choices in `%LOCALAPPDATA%\EdgePlus\settings.json`, in the same place and
shape the earlier Tauri panel used, so an existing install carries over; on its
first start it also moves that panel's start-at-sign-in entry to itself.

## Two projects: a host and a swappable panel

| | what it is | changes need |
|---|---|---|
| `EdgePlus.csproj` (host) | the exe: process, single instance, the window on demand, title bar, `App.xaml` styles, toast registration, `PanelLoader` | a restart |
| `Panel/EdgePlus.Panel.csproj` (panel) | every feature, the settings, Edge and the DLL, and the window's content (built in C#) | **nothing**: swapped in live |

The host loads the panel into a collectible `AssemblyLoadContext` from a
timestamped copy in `%TEMP%\edgeplus-panel\` (never the build output, which
stays free for the next build). Only the panel resolves in that context; WinUI,
CsWinRT and the framework fall through to the host's, so WinUI objects cross.

## One process, window or not

There is one Edge+ process per user (`Program`, `AppInstance`), and it runs
the panel for its whole life. The panel's `BackgroundService` owns the Edge and
theme watchers; the window, while open, only listens to it.

- A **boot launch** (`--background`, from the Run entry that
  `Setup/StartWithWindows` writes) opens no window. It is the always-on
  listener, trimmed to about 19 MB of working set (`ProcessTrim`).
- Launching Edge+ again, or clicking its toast, opens the window on that same
  process.
- Closing the window keeps the listener while Edge+ starts with Windows
  (`PanelEntry.IsResident`), and ends the process otherwise.

## When Edge+ updates

`UpdateWatch` (a `BackgroundService` watcher like the Edge and theme ones)
reads the repository's `docs/updates.json` at start and every six hours,
downloads a newer release's exe into `%LOCALAPPDATA%\EdgePlus\update\`, checks
it against the manifest's digest and tells the window, which shows
`UpdateCard`. Restart to update swaps the running exe in place
(`UpdateInstall`), starts the new one with `--restarted-from <pid>` and exits;
the new one waits for that pid before its single-instance check. Only a
published exe does this (the host's own `edgeplus.dll` is not beside it), and
`Settings.AutoUpdate` turns the check off. `EDGEPLUS_UPDATE_FEED` points it at
a manifest on this machine for a test. The release side is in
`docs/RELEASING.md`.

## When Edge updates

The split DLL runs only on the exact `msedge.dll` build in `offsets.h`.
`verify.h` refuses any other build, because every hook site is an address in
that one build. So support is decided **per whole build**: on any other build,
every DLL feature is paused. `EdgeSupport` is the one place that decides this.

- **Seeing it.** `EdgeWatch` checks the build at start, whenever a new Edge
  browser process appears (a cheap 2 s process-list poll), and when the install
  folder's `msedge.exe` changes (a file watch, no polling). The build it reads
  is the one Edge is running, else the installed one.
- **Pausing.** The user's on/off choice is never rewritten. `DllSync` sends the
  DLL the *effective* features, with paused ones off, so nothing is injected on
  that build. A paused card keeps its switch on but locked, and says "Edge
  updated to 156.0.x. Split into any grid is paused until Edge+ supports this
  version." Once Edge+ supports the build again, the choice applies by itself.
- **Telling.** If no window is open, one Windows toast
  (`AppNotification`) lists the paused features, once per build. The build it
  last reported is kept in `%LOCALAPPDATA%\EdgePlus\paused-toast-build.txt`.

**Simulating an update** without touching Edge: point
`EDGEPLUS_SIMULATE_EDGE_DIR` at a folder that holds a fake `msedge.exe` with
the version you want (any exe with that file version works). Then launch the
panel. In a simulation, only that folder decides the build. Nothing is
injected, `settings.json` and the DLL config are not written, and the toast's
once-per-build record stays in memory.

## The dev loop

```powershell
dotnet build app\panel-winui                          # host + panel, once
app\panel-winui\bin\Debug\net10.0-windows10.0.26100.0\win-x64\edgeplus.exe

# edit anything under Panel\, then:
dotnet build app\panel-winui\Panel\EdgePlus.Panel.csproj
```

The running window swaps the new panel in within about a second: the old panel
is torn down (its watcher threads stopped, every subscription to the host
disposed), its context unloaded, the fresh copy mounted from the current
settings, and the title bar says "Panel updated HH:mm:ss" for a few seconds.
`%TEMP%\edgeplus-panel\swap.log` records whether each old context unloaded.

A run is a **dev run** — and watches `Panel\bin\<Configuration>\…\edgeplus-panel.dll`
— when the exe sits inside the repo checkout it was built from and that output
exists (`PanelSource.IsDevRun`). Build the panel in the host's configuration
(Debug for a Debug host). A published `edgeplus.exe` anywhere else loads the
panel it carries as a resource and watches nothing.

Changes to the host (`Host\`, `App.xaml`, `Program.cs`) still need a restart.

## Rules that keep the panel unloadable

Anything that registers the panel with a process-wide list pins its context,
so an old panel would never unload. Each of these was found with `dotnet-dump
gcroot` on a running host:

- **No compiled XAML, no subclassing WinRT types** in the panel. Build UI in C#
  from WinUI's controls; styles live in the host's `App.xaml` (`StyleKeys`).
- **CsWinRT's AOT optimizer is off** for the panel
  (`CsWinRTAotOptimizerEnabled=false`): it emits a module initializer that
  registers the assembly's lookups globally.
- **System.Text.Json through a source-generated context** (`SettingsJson`), not
  reflection, whose accessor cache is process-wide.
- **Every thread stops and every host subscription is disposed**:
  `PanelView.Dispose` drops the window's ties, and the background service's
  `Dispose` stops the watchers (`PollThread`, `HostSignal`).

## Why this, not .NET Hot Reload

`dotnet watch` / Hot Reload applies edits to the running process, but only
the edits it supports. It rejects "rude edits" (new types, changed signatures,
lambdas that capture differently) and then needs a restart. It also needs the
debugger or `dotnet watch` to own the process. The load-context swap takes any
change that builds and keeps the window open, the same way the split DLL is
hot-reloaded into Edge. It works with nothing attached, and it doesn't depend
on WinUI's XAML Hot Reload, which only Visual Studio provides.

## Tests and lint

```powershell
app\panel-winui\test\run.cmd     # pure-logic tests (no WinUI, no Win32)
pwsh tools\lint.ps1              # C++ and C#
```
