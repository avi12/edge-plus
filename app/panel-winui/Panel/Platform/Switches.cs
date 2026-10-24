using System;

namespace EdgePlus;

internal static class Switches {
  // A boot launch from the Run entry: the panel starts minimized.
  public const string Background = "--background";

  // Re-run headless as the dev-mode waiter (DevModePrompt).
  public const string DevModeWaiter = "--quiet-dev-mode-after-exit";

  // The relaunch after an update, followed by the pid of the process it
  // replaces: the new one waits for it to exit before it looks for a running
  // panel to hand over to.
  public const string RestartedFrom = "--restarted-from";

  public static bool IsPresent(string[] commandLine, string name) =>
      Array.Exists(commandLine, argument => argument == name);

  // The argument after `name`, or null when the switch is absent or last.
  public static string? ValueOf(string[] commandLine, string name) {
    int index = Array.IndexOf(commandLine, name);
    bool hasValue = index >= 0 && index + 1 < commandLine.Length;
    return hasValue ? commandLine[index + 1] : null;
  }
}
