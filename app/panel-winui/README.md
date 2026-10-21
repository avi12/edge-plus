# The WinUI panel

Edge+'s panel as WinUI 3 / C#, modelled on `avi/any-stt`'s app. It reads and
writes the Tauri panel's `settings.json` unchanged, so either panel can run.

## Two projects: a host and a swappable panel

| | what it is | changes need |
|---|---|---|
| `EdgePlus.csproj` (host) | the exe: process, single instance, window, title bar, `App.xaml` styles, `PanelLoader` | a restart |
| `Panel/EdgePlus.Panel.csproj` (panel) | every feature, the settings, Edge and the DLL, and the window's content (built in C#) | **nothing**: swapped in live |

The host loads the panel into a collectible `AssemblyLoadContext` from a
timestamped copy in `%TEMP%\edgeplus-panel\` (never the build output, which
stays free for the next build). Only the panel resolves in that context; WinUI,
CsWinRT and the framework fall through to the host's, so WinUI objects cross.

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
- **Every thread stops and every host subscription is disposed** in
  `PanelView.Dispose` (`PollThread`, `HostSignal`).

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
