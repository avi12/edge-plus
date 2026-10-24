using System;
using System.Diagnostics;
using System.Threading.Tasks;
using Microsoft.Windows.AppLifecycle;

namespace EdgePlus;

// Replaces the generated Main (DISABLE_XAML_GENERATED_MAIN) to decide the kind
// of launch before any WinUI runs.
internal static class Program {
  private const string InstanceKey = "edgeplus.panel";
  private const int ReplacedExitMilliseconds = 30_000;

  // At sign-in: the Run entry passes the switch.
  public static bool IsBootLaunch {
    get; private set;
  }

  [STAThread]
  private static void Main(string[] args) {
    if (Switches.IsPresent(args, Switches.DevModeWaiter)) {
      PanelLoader.RunDevModeWaiter();
      return;
    }

    WaitForReplacedProcess(args);
    IsBootLaunch = Switches.IsPresent(args, Switches.Background);
    if (IsRedirectedToRunningPanel()) {
      return;
    }

    // What the generated Main would run; the synchronization context makes
    // awaits resume on the UI thread.
    WinRT.ComWrappersSupport.InitializeComWrappers();
    Microsoft.UI.Xaml.Application.Start(initialization => {
      var context = new Microsoft.UI.Dispatching.DispatcherQueueSynchronizationContext(
          Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread());
      System.Threading.SynchronizationContext.SetSynchronizationContext(context);
      _ = new App();
    });
  }

  // After an update the new exe starts before the old one has ended, and the
  // old one holds the single-instance key until it does: waiting here, rather
  // than finding it and handing the launch over, is what lets the new one run.
  private static void WaitForReplacedProcess(string[] args) {
    bool isRestart = int.TryParse(Switches.ValueOf(args, Switches.RestartedFrom), out int pid);
    if (!isRestart) {
      return;
    }
    try {
      using Process replaced = Process.GetProcessById(pid);
      replaced.WaitForExit(ReplacedExitMilliseconds);
    } catch (ArgumentException) {
      // Already gone.
    }
  }

  // The redirect is awaited off this STA thread: the activation is a COM call
  // that must not wait on a thread it would block.
  private static bool IsRedirectedToRunningPanel() {
    AppInstance registered = AppInstance.FindOrRegisterForKey(InstanceKey);
    if (registered.IsCurrent) {
      return false;
    }
    // A boot launch finding a panel already running has nothing to add, and
    // redirecting it would open that panel's window.
    if (IsBootLaunch) {
      return true;
    }
    AppActivationArguments activation = AppInstance.GetCurrent().GetActivatedEventArgs();
    Task.Run(() => registered.RedirectActivationToAsync(activation).AsTask()).Wait();
    return true;
  }
}
