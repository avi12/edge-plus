using System;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace EdgePlus;

// The panel's only door, called by the host (PanelLoader) by name through
// reflection: the host never references this assembly's types, so it never
// loads them into its own context, where they could not be unloaded. Every
// parameter and result is a framework or WinUI type both sides share.
public static class PanelEntry {
  // Fill `slot` in the host's `window` with the panel, and start its watchers.
  // `settingChanged` fires when a Windows setting changed; `broughtBack` when
  // a second launch brought the window back. Disposing the result tears all of
  // it down, so the host can unload this assembly and load a newer one.
  public static IDisposable Mount(Window window, ContentControl slot,
                                  IObservable<EventArgs> settingChanged,
                                  IObservable<EventArgs> broughtBack) {
    return new PanelView(window, slot, settingChanged, broughtBack);
  }

  // Start what runs with or without a window (BackgroundService: the Edge and
  // theme watchers, the toast). Disposing the result stops it, so the host can
  // unload this assembly.
  public static IDisposable StartBackground() {
    // Retire the old extension and native-messaging host if a version from
    // before 2026-10-15 left them behind, and unpack the split DLL this build
    // carries before the watcher can inject it. Both are idempotent, so a live
    // swap runs them again harmlessly.
    LegacyCleanup.Run();
    Payload.Unpack();
    MigrateStartupEntry();
    return BackgroundService.Start();
  }

  // A sign-in entry the retired Tauri panel left now starts this panel as
  // the windowless listener, so the Edge update watch runs from sign-in. A
  // packaged panel leaves it alone: it starts through its own startup task.
  private static void MigrateStartupEntry() {
    if (Packaging.IsPackaged) {
      return;
    }
    if (Environment.ProcessPath is not string executable) {
      return;
    }
    try {
      if (StartupEntry.MigrateFromTauri(StartupKeys.User, StartupEntry.Name, executable)) {
        PanelLog.Write($"start at sign-in moved from the Tauri panel to {executable}");
      }
    } catch (Exception error) when (error is UnauthorizedAccessException or System.Security.SecurityException
                                        or System.IO.IOException) {
      PanelLog.Write("start at sign-in could not be moved from the Tauri panel", error);
    }
  }

  // Whether the process stays when its window closes: while Edge+ starts with
  // Windows it is the always-on listener for Edge updates.
  public static bool IsResident() => StartWithWindows.IsEnabled();

  // The headless launch (Switches.DevModeWaiter): wait for Edge to exit, snooze
  // its developer-mode prompt, return.
  public static void RunDevModeWaiter() =>
      DevModePrompt.WaitForEdgeExitThenSnooze(EdgeUserData.Default);
}
