using System;

namespace EdgePlus;

// Runs whether or not the window is open. A window listens to it rather than
// running watchers of its own, so there is one watcher of each and a boot
// launch with no window is still the listener.
internal sealed class BackgroundService : IDisposable {
  private readonly EdgeWatch _edgeWatch = new();
  private readonly ThemeWatch _themeWatch = new();
  private readonly StoreUpdates _storeUpdates;

  private BackgroundService() {
    _storeUpdates = new StoreUpdates(() => IsWindowOpen);
    _edgeWatch.BuildChanged += OnBuildChanged;
    _edgeWatch.BrowserStarted += () => BrowserStarted?.Invoke();
    _themeWatch.Refreshed += refresh => ThemeRefreshed?.Invoke(refresh);
  }

  public static BackgroundService? Current {
    get; private set;
  }

  // Raised on a watcher thread.
  public event Action<BuildChange>? BuildChanged;

  public event Action<ThemeRefresh>? ThemeRefreshed;

  // Raised on a watcher thread once a new Edge browser process got the DLL.
  public event Action? BrowserStarted;

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
    _storeUpdates.Dispose();
    if (Current == this) {
      Current = null;
    }
  }

  // A build that pauses features is fixed by the Edge+ release made for it,
  // so look for one now rather than at the next scheduled check.
  private void OnBuildChanged(BuildChange change) {
    if (change.Paused.Count > 0) {
      _storeUpdates.CheckNow();
    }
    if (!IsWindowOpen) {
      PausedToast.ShowOncePerBuild(change);
    }
    BuildChanged?.Invoke(change);
  }
}
