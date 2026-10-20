using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;

namespace EdgePlus;

// Watching for Edge to (re)start, which is when an update lands.
//
// Edge installs an update into a new version folder and swaps it in when it
// restarts, so the build only changes when a new browser process appears. At
// panel start, and whenever a browser process appears that was not there on the
// previous poll, the running build is checked (Reconcile) before anything is
// injected; features that depend on a replaced msedge.dll are switched off and
// the UI is told. Watching for new pids rather than for "none to some" also
// catches a restart quicker than one poll. The Tauri panel's edge_watch.rs.
internal sealed class EdgeWatch {
  private static readonly TimeSpan s_pollInterval = TimeSpan.FromSeconds(2);

  // Raised on the watch thread with each newly seen build (and what it switched
  // off, often nothing). The window subscribes before Start, so unlike the
  // Tauri panel no change can be missed before the UI listens.
  public event Action<BuildChange>? BuildChanged;

  public void Start() {
    var thread = new Thread(Run) { IsBackground = true, Name = "Edge watch" };
    thread.Start();
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

  // Look at the Edge that is running: remember its build, and when it is not
  // the pinned one switch off (and persist) every feature that depends on it,
  // before anything is injected. Returns the change when the build is a new one
  // or something was switched off.
  public static BuildChange? Reconcile() {
    FileVersion? running = RunningBuild();
    if (running is not FileVersion build) {
      return null;
    }
    lock (Settings.Gate) {
      Settings settings = Settings.Load();
      FileVersion? previous = FileVersion.Parse(settings.LastEdgeBuild);
      IReadOnlyList<Feature> disabled = EdgeBuild.Blocked(settings.Features, build,
                                                          EdgeBuild.Pinned);
      bool isNewBuild = previous != build;
      bool isAlreadyHandled = disabled.Count == 0 && !isNewBuild;
      if (isAlreadyHandled) {
        return null;
      }
      foreach (Feature feature in disabled) {
        settings.Features.Set(feature, false);
      }
      settings.LastEdgeBuild = build.ToString();
      settings.Save();
      if (disabled.Count > 0) {
        // Write the switched-off state to the DLL's config and unload any copy
        // still loaded, so nothing is left pointed at the wrong build.
        DllSync.Apply(settings);
      }
      return new BuildChange(build, previous, EdgeBuild.Pinned, disabled);
    }
  }

  private void Run() {
    Check();
    DllSync.SyncOnStart();
    var known = new HashSet<int>(EdgeProcesses.BrowserPids());
    while (true) {
      Thread.Sleep(s_pollInterval);
      var current = new HashSet<int>(EdgeProcesses.BrowserPids());
      bool hasNewProcess = !current.IsSubsetOf(known);
      if (hasNewProcess) {
        Check();
      }
      known = current;
    }
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
