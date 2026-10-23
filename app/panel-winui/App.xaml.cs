using System;
using System.IO;
using System.Runtime.InteropServices;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.Windows.AppLifecycle;
using Microsoft.Windows.AppNotifications;

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
  private static readonly TimeSpan s_trimDelay = TimeSpan.FromSeconds(5);

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
    RegisterForToasts(dispatcher);
    _loader = new PanelLoader(dispatcher);
    _loader.Start();
    // A second launch redirected here (Program) shows the window.
    AppInstance.GetCurrent().Activated += (_, _) => dispatcher.TryEnqueue(ShowWindow);
    // The plain exe boots with the Run entry's switch; the Store package's
    // startup task passes no arguments, only its activation kind.
    bool isBootLaunch =
        Switches.IsPresent(Environment.GetCommandLineArgs(), Switches.Background)
        || AppInstance.GetCurrent().GetActivatedEventArgs().Kind == ExtendedActivationKind.StartupTask;
    if (isBootLaunch) {
      TrimWhenSettled(dispatcher);
    } else {
      ShowWindow();
    }
  }

  // Give memory back once the startup (or the closed window's teardown) has
  // settled, unless a window opened meanwhile.
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

  // The panel's toast (PausedToast) needs the process registered; clicking it
  // opens the window. A toast clicked while no Edge+ runs starts a new one,
  // which opens its window as any plain launch does. The handler is attached
  // before Register, as the Windows App SDK requires.
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
