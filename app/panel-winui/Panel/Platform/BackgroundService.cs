using System;

namespace EdgePlus;

// Runs whether or not the window is open. A window listens to it rather than
// running watchers of its own, so there is one watcher of each and a boot
// launch with no window is still the listener.
internal sealed class BackgroundService : IDisposable {
  private readonly EdgeWatch _edgeWatch = new();
  private readonly ThemeWatch _themeWatch = new();

  private BackgroundService() {
    _edgeWatch.BuildChanged += OnBuildChanged;
    _edgeWatch.BrowsersChanged += () => BrowsersChanged?.Invoke();
    _themeWatch.Refreshed += refresh => ThemeRefreshed?.Invoke(refresh);
  }

  public static BackgroundService? Current {
    get; private set;
  }

  // Raised on a watcher thread.
  public event Action<BuildChange>? BuildChanged;

  public event Action<ThemeRefresh>? ThemeRefreshed;

  // Raised on a watcher thread when an Edge browser process starts (once it got
  // the DLL, with auto-inject on) or closes: the window re-reads its state.
  public event Action? BrowsersChanged;

  // Set by PanelView; a toast is only for when no window is open.
  public bool IsWindowOpen {
    get; set;
  }

  public static BackgroundService Start() {
    var service = new BackgroundService();
    Current = service;
    service._edgeWatch.Start();
    service._themeWatch.Start();
    return service;
  }

  public void Dispose() {
    _edgeWatch.Stop();
    _themeWatch.Stop();
    if (Current == this) {
      Current = null;
    }
  }

  private void OnBuildChanged(BuildChange change) {
    if (!IsWindowOpen) {
      PausedToast.ShowOncePerBuild(change);
    }
    BuildChanged?.Invoke(change);
  }
}
