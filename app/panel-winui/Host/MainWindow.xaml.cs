using System;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Windows.Graphics;

namespace EdgePlus;

// Nothing of the panel is compiled in here: PanelLoader fills PanelSlot.
public sealed partial class MainWindow : Window {
  // In device-independent pixels; only the height is the user's.
  private const int WindowWidth = 560;
  private const int WindowHeight = 640;
  private const int MinimumHeight = 420;
  private const double BaseDpi = 96.0;
  private const double StatusShown = 1;
  private const double StatusHidden = 0;
  private static readonly TimeSpan s_statusTime = TimeSpan.FromSeconds(4);

  private readonly HostSignal _settingChanged = new();
  private readonly HostSignal _broughtBack = new();
  private readonly PanelLoader _loader;
  private readonly DispatcherQueueTimer _statusTimer;

  internal MainWindow(PanelLoader loader) {
    InitializeComponent();
    WindowChrome.Apply(this, AppTitleBar);
    FixWidth();
    WindowChrome.CenterOnScreen(AppWindow);
    WindowSubclass.SettingChanged += _settingChanged.Raise;

    _statusTimer = DispatcherQueue.CreateTimer();
    _statusTimer.Interval = s_statusTime;
    _statusTimer.IsRepeating = false;
    _statusTimer.Tick += (_, _) => SwapStatus.Opacity = StatusHidden;

    _loader = loader;
    _loader.Swapped += ShowSwapStatus;
    _loader.Show(this, PanelSlot, _settingChanged, _broughtBack);
    Closed += (_, _) => {
      _loader.Swapped -= ShowSwapStatus;
      WindowSubclass.SettingChanged -= _settingChanged.Raise;
      _statusTimer.Stop();
      _loader.Hide();
    };
  }

  // Launching Edge+ again lands here.
  public void BringBack() {
    if (AppWindow.Presenter is OverlappedPresenter presenter) {
      presenter.Restore();
    }
    Activate();
    _broughtBack.Raise();
  }

  private void ShowSwapStatus(string line) {
    SwapStatus.Text = line;
    SwapStatus.Opacity = StatusShown;
    _statusTimer.Stop();
    _statusTimer.Start();
  }

  private void FixWidth() {
    double scale = WindowDpi.Of(this) / BaseDpi;
    int width = (int)Math.Round(WindowWidth * scale);
    AppWindow.Resize(new SizeInt32(width, (int)Math.Round(WindowHeight * scale)));
    if (AppWindow.Presenter is OverlappedPresenter presenter) {
      presenter.IsMaximizable = false;
      presenter.PreferredMinimumWidth = width;
      presenter.PreferredMaximumWidth = width;
      presenter.PreferredMinimumHeight = (int)Math.Round(MinimumHeight * scale);
    }
    // Hides the horizontal resize cursor, and reports Windows setting changes.
    _ = WindowSubclass.Attach(this);
  }
}
