using System;
using Microsoft.UI.Windowing;
using Microsoft.UI.Xaml;
using Windows.Graphics;

namespace EdgePlus;

public sealed partial class MainWindow : Window {
  // In device-independent pixels, the Tauri window's sizes: the width is fixed
  // and only the height is the user's.
  private const int WindowWidth = 560;
  private const int WindowHeight = 640;
  private const int MinimumHeight = 420;
  private const double BaseDpi = 96.0;

  public MainWindow() {
    InitializeComponent();
    WindowChrome.Apply(this, AppTitleBar);
    FixWidth();
    WindowChrome.CenterOnScreen(AppWindow);
  }

  // Launching Edge+ again brings the running panel back.
  public void BringBack() {
    if (AppWindow.Presenter is OverlappedPresenter presenter) {
      presenter.Restore();
    }
    Activate();
  }

  public void Minimize() {
    if (AppWindow.Presenter is OverlappedPresenter presenter) {
      presenter.Minimize();
    }
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
  }
}
