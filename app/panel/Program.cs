using System;

namespace FixEdge;

// The Windows App SDK's generated Main, written out because the csproj
// suppresses it (DISABLE_XAML_GENERATED_MAIN) so the app owns its entry point.
internal static class Program {
  [STAThread]
  private static void Main() {
    WinRT.ComWrappersSupport.InitializeComWrappers();
    Microsoft.UI.Xaml.Application.Start(_ => {
      var context = new Microsoft.UI.Dispatching.DispatcherQueueSynchronizationContext(
          Microsoft.UI.Dispatching.DispatcherQueue.GetForCurrentThread());
      System.Threading.SynchronizationContext.SetSynchronizationContext(context);
      new App();
    });
  }
}
