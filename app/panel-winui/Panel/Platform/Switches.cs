using System;

namespace EdgePlus;

// Every switch edgeplus.exe answers on its own command line, and the one way
// they are matched, so the string a launcher WRITES (the Run entry, the
// dev-mode waiter) and the string the launch READS are the same constant.
internal static class Switches {
  // A boot launch from the Run entry: the panel starts minimized.
  public const string Background = "--background";

  // Re-run headless as the dev-mode waiter (DevModePrompt), never showing a
  // window. Same switch the Tauri panel uses, so either panel's waiter is one.
  public const string DevModeWaiter = "--quiet-dev-mode-after-exit";

  public static bool IsPresent(string[] commandLine, string name) =>
      Array.Exists(commandLine, argument => argument == name);
}
