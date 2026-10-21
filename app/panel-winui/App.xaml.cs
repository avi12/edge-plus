using System;
using System.IO;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.Windows.AppLifecycle;

namespace EdgePlus;

// The process: one per user (Program), running the panel for its whole life.
//
// A boot launch (--background, from the start-at-sign-in entry) opens no
// window: the process is the always-on listener — the panel's background
// service watches Edge for updates and says by toast what an unsupported build
// paused. Launching Edge+ again (or clicking the toast) opens the window on the
// same running panel. Closing the window keeps the listener while Edge+ starts
// with Windows, and ends the process otherwise.
public partial class App : Application {
  private const string CrashLogName = "edgeplus_error.log";

  private PanelLoader? _loader;
  // The one window, while it is open.
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
    // The process ends when the panel says so (OnWindowClosed), not when the
    // last window closes: a boot launch has no window at all.
    DispatcherShutdownMode = DispatcherShutdownMode.OnExplicitShutdown;
  }

  protected override void OnLaunched(LaunchActivatedEventArgs args) {
    DispatcherQueue dispatcher = DispatcherQueue.GetForCurrentThread();
    _loader = new PanelLoader(dispatcher);
    _loader.Start();
    // A second launch redirected here (Program) shows the window.
    AppInstance.GetCurrent().Activated += (_, _) => dispatcher.TryEnqueue(ShowWindow);
    bool isBootLaunch = Switches.IsPresent(Environment.GetCommandLineArgs(), Switches.Background);
    if (!isBootLaunch) {
      ShowWindow();
    }
  }

  // Open the window, or bring the open one back.
  public void ShowWindow() {
    if (_window is not null) {
      _window.BringBack();
      return;
    }
    _window = new MainWindow(_loader!);
    _window.Closed += OnWindowClosed;
    _window.Activate();
  }

  private void OnWindowClosed(object sender, WindowEventArgs args) {
    _window = null;
    if (!_loader!.IsResident()) {
      Exit();
    }
  }
}
