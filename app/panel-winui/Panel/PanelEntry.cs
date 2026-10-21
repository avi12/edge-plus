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
    // Retire the old extension and native-messaging host if a version from
    // before 2026-10-15 left them behind, and unpack the split DLL this build
    // carries before anything injects it. Both are idempotent, so a live swap
    // runs them again harmlessly.
    LegacyCleanup.Run();
    Payload.Unpack();
    return new PanelView(window, slot, settingChanged, broughtBack);
  }

  // The headless launch (Switches.DevModeWaiter): wait for Edge to exit, snooze
  // its developer-mode prompt, return.
  public static void RunDevModeWaiter() =>
      DevModePrompt.WaitForEdgeExitThenSnooze(EdgeUserData.Default);
}
