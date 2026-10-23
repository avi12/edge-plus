using System;

namespace EdgePlus;

internal static class Switches {
  // A boot launch from the Run entry: the panel starts minimized.
  public const string Background = "--background";

  // Re-run headless as the dev-mode waiter (DevModePrompt).
  public const string DevModeWaiter = "--quiet-dev-mode-after-exit";

  public static bool IsPresent(string[] commandLine, string name) =>
      Array.Exists(commandLine, argument => argument == name);
}
