using System;

namespace EdgePlus;

// Runs whether or not the window is open. A window listens to it rather than
// running watchers of its own, so there is one watcher of each and a boot
// launch with no window is still the listener.
internal sealed class BackgroundService : IDisposable {
  private readonly EdgeWatch _edgeWatch = new();
  private readonly ThemeWatch _themeWatch = new();
  private readonly UpdateWatch _updateWatch = new();

  private BackgroundService() {
    _edgeWatch.BuildChanged += OnBuildChanged;
    _edgeWatch.BrowsersChanged += () => BrowsersChanged?.Invoke();
    _themeWatch.Refreshed += refresh => ThemeRefreshed?.Invoke(refresh);
    _updateWatch.Ready += ready => UpdateReady?.Invoke(ready);
  }

  public static BackgroundService? Current {
    get; private set;
  }

  // Raised on a watcher thread.
  public event Action<BuildChange>? BuildChanged;

  public event Action<ThemeRefresh>? ThemeRefreshed;

  // Raised on a watcher thread once a newer release is downloaded and checked.
  public event Action<ReadyUpdate>? UpdateReady;

  // Raised on a watcher thread when an Edge browser process starts (once it got
  // the DLL, with auto-inject on) or closes: the window re-reads its state.
  public event Action? BrowsersChanged;

  // The downloaded release waiting for a restart, if any.
  public ReadyUpdate? PendingUpdate => _updateWatch.Pending;

  // Set by PanelView; a toast is only for when no window is open.
  public bool IsWindowOpen {
    get; set;
  }

  public static BackgroundService Start() {
    var service = new BackgroundService();
    Current = service;
    service._edgeWatch.Start();
    service._themeWatch.Start();
    service._updateWatch.Start();
    return service;
  }

  // The switch was turned on: look now rather than at the next scheduled check.
  public void CheckForUpdateSoon() => _updateWatch.CheckSoon();

  public void Dispose() {
    _edgeWatch.Stop();
    _themeWatch.Stop();
    _updateWatch.Stop();
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
