using System;
using System.Linq;

namespace FixEdge;

// The Windows App SDK's generated Main, written out because the csproj
// suppresses it (DISABLE_XAML_GENERATED_MAIN) so the app owns its entry point.
internal static class Program {
  [STAThread]
  private static void Main() {
    // The panel is also its own quiet-dev-mode waiter: armed with this switch,
    // it waits for Edge to exit, snoozes the prompt and quits, without ever
    // showing the window. (Re-homed off the retired native-messaging host.)
    string[] arguments = Environment.GetCommandLineArgs();
    bool isDevModeWaiter =
        arguments.Contains(DevModePrompt.AfterExitSwitch, StringComparer.Ordinal);
    if (isDevModeWaiter) {
      DevModePrompt.WaitForEdgeExitThenSnooze(arguments);
      return;
    }

    WinRT.ComWrappersSupport.InitializeComWrappers();
    Microsoft.UI.Xaml.Application.Start(_ => {
      var context = new Microsoft.UI.Dispatching.DispatcherQueueSynchronizationContext(
          Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread());
      System.Threading.SynchronizationContext.SetSynchronizationContext(context);
      new App();
    });
  }
}
