using System;
using System.IO;
using System.Threading;
using Microsoft.UI.Xaml;

namespace FixEdge;

public partial class App : Application {
  private const string InstanceMutexName = @"Local\fix-edge.control.singleton";
  private const string CrashLogName = "fixedge_error.log";

  // Held for their lifetime, not their value: releasing the mutex would let a
  // second panel start, and the window is the app's one root.
  private Mutex? _singleInstance;
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
    _singleInstance = new Mutex(true, InstanceMutexName, out bool isFirstInstance);
    if (!isFirstInstance) {
      Exit();
      return;
    }

    // Retire the old extension + native-messaging host if a previous version
    // left them behind, then unpack the native split DLL this build carries.
    LegacyCleanup.Run();
    NativeSplitPayload.Unpack();

    _window = new MainWindow();
    _window.Activate();
  }
}
