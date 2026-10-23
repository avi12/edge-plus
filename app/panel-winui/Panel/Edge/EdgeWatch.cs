using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

namespace EdgePlus;

// Edge swaps an update in only when it restarts, so the build can only change
// when a new browser process appears or the launcher is swapped: both trigger a
// build check before anything is injected.
internal sealed class EdgeWatch {
  private static readonly TimeSpan s_pollInterval = TimeSpan.FromSeconds(2);

  // Raised on the watch thread. Subscribe before Start, or a change is missed.
  public event Action<BuildChange>? BuildChanged;

  private readonly PollThread _thread = new("Edge watch", s_pollInterval);
  private HashSet<int> _knownBrowserPids = [];

  private System.IO.FileSystemWatcher? _installWatch;

  public void Start() => _thread.Start(Begin, Poll);

  public void Stop() {
    _installWatch?.Dispose();
    _installWatch = null;
    _thread.Stop();
  }

  public static FileVersion? BuildOf(int pid) =>
      EdgeProcesses.ModulePath(pid, Shared.EdgeModule) is string path
          ? FileVersion.Of(path)
          : null;

  public static FileVersion? RunningBuild() {
    FileVersion[] builds = EdgeProcesses.BrowserPids()
                               .Select(BuildOf)
                               .OfType<FileVersion>()
                               .ToArray();
    return EdgeBuild.Judged(builds, EdgeBuild.Pinned);
  }

  // The running build, else the installed one Edge will run when it next starts.
  public static FileVersion? CurrentBuild() {
    if (EdgeInstall.SimulatedDirectory is string simulated) {
      return EdgeInstall.InstalledBuild(simulated);
    }
    return RunningBuild() ?? EdgeInstall.InstalledBuild(EdgeInstall.ApplicationDirectory);
  }

  // Records a new build and brings the DLL in line with it; null when the build
  // is unchanged.
  public static BuildChange? Reconcile() {
    if (CurrentBuild() is not FileVersion build) {
      return null;
    }
    lock (Settings.Gate) {
      Settings settings = Settings.Load();
      FileVersion? previous = FileVersion.Parse(settings.LastEdgeBuild);
      bool isNewBuild = previous != build;
      if (!isNewBuild) {
        return null;
      }
      if (!EdgeInstall.IsSimulated) {
        settings.LastEdgeBuild = build.ToString();
        settings.Save();
        DllSync.Apply(settings);
      }
      IReadOnlyList<Feature> paused = EdgeSupport.Paused(settings.Features, build,
                                                         EdgeBuild.Pinned);
      return new BuildChange(build, previous, EdgeBuild.Pinned, paused);
    }
  }

  private void Begin() {
    Check();
    DllSync.SyncOnStart();
    _knownBrowserPids = new HashSet<int>(EdgeProcesses.BrowserPids());
    WatchInstall();
  }

  // Catches an update applied while Edge is closed.
  private void WatchInstall() {
    string? folder = EdgeInstall.ApplicationDirectory;
    if (folder is null || !System.IO.Directory.Exists(folder)) {
      return;
    }
    _installWatch = new System.IO.FileSystemWatcher(folder, EdgeInstall.LauncherName) {
      NotifyFilter = System.IO.NotifyFilters.LastWrite | System.IO.NotifyFilters.FileName
          | System.IO.NotifyFilters.Size,
    };
    _installWatch.Changed += (_, _) => Check();
    _installWatch.Created += (_, _) => Check();
    _installWatch.Renamed += (_, _) => Check();
    _installWatch.EnableRaisingEvents = true;
  }

  private void Poll() {
    var current = new HashSet<int>(EdgeProcesses.BrowserPids());
    bool hasNewProcess = !current.IsSubsetOf(_knownBrowserPids);
    if (hasNewProcess) {
      Check();
    }
    _knownBrowserPids = current;
  }

  // A settings file that cannot be written is tried again on the next new
  // process; an exception out of a background thread would end the panel.
  private void Check() {
    BuildChange? change;
    try {
      change = Reconcile();
    } catch (System.IO.IOException) {
      return;
    } catch (UnauthorizedAccessException) {
      return;
    }
    if (change is not null) {
      BuildChanged?.Invoke(change);
    }
  }
}
