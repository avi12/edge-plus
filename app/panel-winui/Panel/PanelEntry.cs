using System;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace EdgePlus;

// Called by PanelLoader by name through reflection, so the host never loads
// these types into its own context. Every parameter and result is a framework
// or WinUI type both sides share. Disposing a result tears it down so the host
// can unload this assembly.
public static class PanelEntry {
  public static IDisposable Mount(Window window, ContentControl slot,
                                  IObservable<EventArgs> settingChanged,
                                  IObservable<EventArgs> broughtBack) {
    return new PanelView(window, slot, settingChanged, broughtBack);
  }

  public static IDisposable StartBackground() {
    // Idempotent, so a live swap runs them again harmlessly. The DLL is
    // unpacked before the watcher can inject it.
    LegacyCleanup.Run();
    Payload.Unpack();
    MigrateStartupEntry();
    return BackgroundService.Start();
  }

  private static void MigrateStartupEntry() {
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

  // While Edge+ starts with Windows, the process outlives its window as the
  // listener for Edge updates.
  public static bool IsResident() => StartWithWindows.IsEnabled();

  public static void RunDevModeWaiter() =>
      DevModePrompt.WaitForEdgeExitThenSnooze(EdgeUserData.Default);
}
