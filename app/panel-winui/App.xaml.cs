using System;
using System.IO;
using Microsoft.UI.Xaml;
using Microsoft.Windows.AppLifecycle;

namespace EdgePlus;

public partial class App : Application {
  private const string CrashLogName = "edgeplus_error.log";

  // Held for its lifetime: the window is the app's one root.
  private MainWindow? _window;

  public App() {
    InitializeComponent();
    UnhandledException += (_, e) => {
      try {
        File.WriteAllText(Path.Combine(Path.GetTempPath(), CrashLogName),
                          e.Message + "\n\n" + e.Exception);
      } catch (IOException) {
      }
    };
  }

  protected override void OnLaunched(LaunchActivatedEventArgs args) {
    string[] commandLine = Environment.GetCommandLineArgs();
    // Retire the old extension and native-messaging host if a version from
    // before 2026-10-15 left them behind.
    LegacyCleanup.Run();
    // Unpack the split DLL this build carries before anything injects it.
    Payload.Unpack();
    _window = new MainWindow();
    // A second launch redirected here (Program) brings this window back.
    AppInstance.GetCurrent().Activated += (_, _) =>
        _window.DispatcherQueue.TryEnqueue(_window.BringBack);
    _window.Activate();
    // A boot launch keeps the panel in the taskbar instead of in the user's
    // face; it still applies the settings.
    if (Switches.IsPresent(commandLine, Switches.Background)) {
      _window.Minimize();
    }
  }
}
