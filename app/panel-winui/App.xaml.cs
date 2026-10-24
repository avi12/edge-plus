using System;
using System.IO;
using System.Runtime.InteropServices;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.Windows.AppLifecycle;
using Microsoft.Windows.AppNotifications;

namespace EdgePlus;

// A boot launch opens no window: the process is the always-on listener that
// watches Edge for updates. Closing the window keeps the listener while Edge+
// starts with Windows, and ends the process otherwise.
public partial class App : Application {
  private const string CrashLogName = "edgeplus_error.log";
  private static readonly TimeSpan s_trimDelay = TimeSpan.FromSeconds(5);

  private PanelLoader? _loader;
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
    // A boot launch has no window at all, so the panel decides when to exit.
    DispatcherShutdownMode = DispatcherShutdownMode.OnExplicitShutdown;
  }

  protected override void OnLaunched(LaunchActivatedEventArgs args) {
    DispatcherQueue dispatcher = DispatcherQueue.GetForCurrentThread();
    RegisterForToasts(dispatcher);
    _loader = new PanelLoader(dispatcher);
    _loader.Start();
    // A second launch, redirected here by Program.
    AppInstance.GetCurrent().Activated += (_, _) => dispatcher.TryEnqueue(ShowWindow);
    if (!Program.IsBootLaunch) {
      ShowWindow();
      return;
    }
    // A relaunch after an update is a boot launch too, and needs no listener
    // when Edge+ no longer starts with Windows.
    if (!_loader.IsResident()) {
      Exit();
      return;
    }
    TrimWhenSettled(dispatcher);
  }

  private void TrimWhenSettled(DispatcherQueue dispatcher) {
    DispatcherQueueTimer timer = dispatcher.CreateTimer();
    timer.Interval = s_trimDelay;
    timer.IsRepeating = false;
    timer.Tick += (_, _) => {
      if (_window is null) {
        ProcessTrim.Run();
      }
    };
    timer.Start();
  }

  public void ShowWindow() {
    if (_window is not null) {
      _window.BringBack();
      return;
    }
    _window = new MainWindow(_loader!);
    _window.Closed += OnWindowClosed;
    _window.Activate();
  }

  // The Windows App SDK requires the handler attached before Register.
  private void RegisterForToasts(DispatcherQueue dispatcher) {
    try {
      AppNotificationManager.Default.NotificationInvoked +=
          (_, _) => dispatcher.TryEnqueue(ShowWindow);
      AppNotificationManager.Default.Register();
    } catch (COMException) {
      // No toasts on this system; the cards still say what is paused.
    }
  }

  private void OnWindowClosed(object sender, WindowEventArgs args) {
    _window = null;
    if (!_loader!.IsResident()) {
      Exit();
      return;
    }
    TrimWhenSettled(DispatcherQueue.GetForCurrentThread());
  }
}
