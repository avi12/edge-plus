using System;

namespace EdgePlus;

// Start at sign-in is the Run key entry for the current user, which points at
// the exe that is running.
internal static class StartWithWindows {
  public static bool IsEnabled() => StartupEntry.IsEnabled(StartupKeys.User, StartupEntry.Name);

  public static void Set(bool isOn) {
    if (isOn) {
      string executable = Environment.ProcessPath
          ?? throw new InvalidOperationException("The panel cannot name its own executable.");
      StartupEntry.Enable(StartupKeys.User, StartupEntry.Name, StartupEntry.RunCommand(executable));
    } else {
      StartupEntry.Disable(StartupKeys.User, StartupEntry.Name);
    }
  }
}
