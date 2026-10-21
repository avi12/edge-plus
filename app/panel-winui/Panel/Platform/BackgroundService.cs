using System;

namespace EdgePlus;

// What the panel does whether or not its window is open, for the life of the
// process: watch Edge for a new build (EdgeWatch) and local themes for edits
// (ThemeWatch). A window, when open, listens to it rather than running watchers
// of its own, so there is one watcher of each, and a boot launch with no window
// is still the listener. When a new build pauses features while no window is
// open, the user is told once by toast (PausedToast).
internal sealed class BackgroundService : IDisposable {
  private readonly EdgeWatch _edgeWatch = new();
  private readonly ThemeWatch _themeWatch = new();

  private BackgroundService() {
    _edgeWatch.BuildChanged += OnBuildChanged;
    _themeWatch.Refreshed += refresh => ThemeRefreshed?.Invoke(refresh);
  }

  // The service of this process (of this panel load), while it runs.
  public static BackgroundService? Current {
    get; private set;
  }

  // Raised on a watcher thread.
  public event Action<BuildChange>? BuildChanged;

  public event Action<ThemeRefresh>? ThemeRefreshed;

  // Whether a window shows the panel now (PanelView says so); a toast is only
  // for when none does.
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
