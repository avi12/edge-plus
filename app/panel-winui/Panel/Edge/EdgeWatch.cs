using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

namespace EdgePlus;

// Watching for Edge to (re)start, which is when an update lands.
//
// Edge installs an update into a new version folder and swaps it in when it
// restarts, so the build only changes when a new browser process appears or
// the launcher is swapped. At start, whenever a browser process appears that
// was not there on the previous (cheap, 2 s) window poll, and whenever the
// install folder's launcher changes, the build is checked (Reconcile) before
// anything is injected; features a different msedge.dll would break are
// paused (EdgeSupport) and the UI is told. The Tauri panel's edge_watch.rs.
internal sealed class EdgeWatch {
  private static readonly TimeSpan s_pollInterval = TimeSpan.FromSeconds(2);

  // Raised on the watch thread with each newly seen build (and what it switched
  // off, often nothing). The window subscribes before Start, so unlike the
  // Tauri panel no change can be missed before the UI listens.
  public event Action<BuildChange>? BuildChanged;

  private readonly PollThread _thread = new("Edge watch", s_pollInterval);
  // The browser processes seen on the last poll.
  private HashSet<int> _known = [];

  private System.IO.FileSystemWatcher? _installWatch;

  public void Start() => _thread.Start(Begin, Poll);

  public void Stop() {
    _installWatch?.Dispose();
    _installWatch = null;
    _thread.Stop();
  }

  // The msedge.dll build browser process `pid` loaded, or null when it cannot
  // be read.
  public static FileVersion? BuildOf(int pid) =>
      EdgeProcesses.ModulePath(pid, Shared.EdgeModule) is string path
          ? FileVersion.Of(path)
          : null;

  // The build the running Edge is judged by (EdgeBuild.Judged), or null when
  // Edge is not running.
  public static FileVersion? RunningBuild() {
    FileVersion[] builds = EdgeProcesses.BrowserPids()
                               .Select(BuildOf)
                               .OfType<FileVersion>()
                               .ToArray();
    return EdgeBuild.Judged(builds, EdgeBuild.Pinned);
  }

  // The build Edge is on now: the running one when Edge runs (any process on a
  // build other than the pinned one decides), else the installed one, which is
  // what Edge will run when it next starts. In a simulation (EdgeInstall) only
  // the fake install counts.
  public static FileVersion? CurrentBuild() {
    if (EdgeInstall.SimulatedDirectory is string simulated) {
      return EdgeInstall.InstalledBuild(simulated);
    }
    return RunningBuild() ?? EdgeInstall.InstalledBuild(EdgeInstall.ApplicationDirectory);
  }

  // Look at the Edge build: remember it, and when it is a new one say which of
  // the user's features it pauses (EdgeSupport). The user's choice is never
  // rewritten; the DLL is brought in line, which on an unsupported build means
  // nothing is injected and a loaded copy is unloaded. Returns the change when
  // the build is a new one.
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
      // A simulation writes nothing: the settings and the DLL stay as they are.
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
    _known = new HashSet<int>(EdgeProcesses.BrowserPids());
    WatchInstall();
  }

  // An update applied while Edge is closed (its launcher swapped) is seen
  // without polling: a watch on the install folder's launcher.
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
    bool hasNewProcess = !current.IsSubsetOf(_known);
    if (hasNewProcess) {
      Check();
    }
    _known = current;
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
