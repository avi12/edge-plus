using System;
using System.Threading.Tasks;
using Microsoft.Windows.AppLifecycle;

namespace EdgePlus;

// The process entry point, ahead of WinUI: the Windows App SDK's generated Main
// is suppressed (DISABLE_XAML_GENERATED_MAIN in the csproj) so this can decide
// what kind of launch this is before a line of WinUI runs.
internal static class Program {
  // Every launch of the panel registers under this key, so a second launch
  // finds the first and hands its activation over instead of opening a window.
  private const string InstanceKey = "edgeplus.panel";

  [STAThread]
  private static void Main(string[] args) {
    // Armed with this switch the panel is its own dev-mode waiter: it waits for
    // Edge to exit, snoozes the prompt and quits, never showing a window.
    if (Switches.IsPresent(args, Switches.DevModeWaiter)) {
      PanelLoader.RunDevModeWaiter();
      return;
    }

    if (IsRedirectedToRunningPanel()) {
      return;
    }

    // The Windows App SDK startup the generated Main would run: initialise
    // CsWinRT, then hand the UI thread to the App with a DispatcherQueue
    // synchronization context so awaits resume on it.
    WinRT.ComWrappersSupport.InitializeComWrappers();
    Microsoft.UI.Xaml.Application.Start(initialization => {
      var context = new Microsoft.UI.Dispatching.DispatcherQueueSynchronizationContext(
          Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread());
      System.Threading.SynchronizationContext.SetSynchronizationContext(context);
      _ = new App();
    });
  }

  // Launching Edge+ again brings the running panel back (App listens for the
  // redirected activation). The redirect is awaited off this STA thread: the
  // activation is a COM call that must not wait on a thread it would block.
  private static bool IsRedirectedToRunningPanel() {
    AppInstance registered = AppInstance.FindOrRegisterForKey(InstanceKey);
    if (registered.IsCurrent) {
      return false;
    }
    AppActivationArguments activation = AppInstance.GetCurrent().GetActivatedEventArgs();
    Task.Run(() => registered.RedirectActivationToAsync(activation).AsTask()).Wait();
    return true;
  }
}
