using System;
using System.Linq;

namespace FixEdge;

internal static class Program {
  private static void Main(string[] args) {
    bool isDevModeWaiter = args.Contains(DevModePrompt.AfterExitSwitch, StringComparer.Ordinal);
    if (isDevModeWaiter) {
      DevModePrompt.WaitForEdgeExitThenSnooze(args);
      return;
    }
    // Each Edge session leaves one waiter behind; it decides at Edge's exit
    // whether the switch is still on.
    if (FeatureSettings.IsEnabled(Catalog.QuietDevMode)) {
      DevModePrompt.Arm();
    }
    NativeHost.Run();
  }
}
