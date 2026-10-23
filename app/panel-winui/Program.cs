using System;
using System.Threading.Tasks;
using Microsoft.Windows.AppLifecycle;

namespace EdgePlus;

// Replaces the generated Main (DISABLE_XAML_GENERATED_MAIN) to decide the kind
// of launch before any WinUI runs.
internal static class Program {
  private const string InstanceKey = "edgeplus.panel";

  // At sign-in or after an update. The Run entry and the restart registration
  // pass the switch; the Store package's startup task passes only its
  // activation kind.
  public static bool IsBootLaunch {
    get; private set;
  }

  [STAThread]
  private static void Main(string[] args) {
    if (Switches.IsPresent(args, Switches.DevModeWaiter)) {
      PanelLoader.RunDevModeWaiter();
      return;
    }

    IsBootLaunch = Switches.IsPresent(args, Switches.Background)
        || AppInstance.GetCurrent().GetActivatedEventArgs().Kind == ExtendedActivationKind.StartupTask;
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
