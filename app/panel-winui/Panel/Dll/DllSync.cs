using System.Linq;
using System.Threading;

namespace EdgePlus;

// The one place that brings the running Edge in line with the settings. The DLL
// only goes into a process on the pinned build and is sent the effective
// features (a paused one as off), so on an unsupported build nothing is injected
// and a loaded copy unloads. A simulation (EdgeInstall) never touches Edge.
internal static class DllSync {
  private const int UnloadPolls = 40;
  private const int UnloadPollMilliseconds = 100;
  private const int LoadPolls = 30;
  private const int LoadPollMilliseconds = 100;

  // Which browser processes an Apply may inject into.
  private enum Reach {
    EveryBrowser,
    Holders,
  }

  // A settings change: with auto-inject off it reaches only the processes that
  // already hold the DLL, and an Edge without one waits for the Inject button.
  public static void Apply(Settings settings) {
    if (settings.AutoInject) {
      Apply(settings, Reach.EveryBrowser, []);
      return;
    }
    Apply(settings, Reach.Holders, EdgeProcesses.PidsWithModule(Paths.SplitDllName));
  }

  private static void Apply(Settings settings, Reach reach, int[] holders) {
    if (EdgeInstall.IsSimulated) {
      return;
    }
    Features effective = Effective(settings);
    WriteConfigIfPossible(settings, effective);
    if (!effective.NeedsDll) {
      DllChannel.SignalUnload();
      return;
    }
    // The unload event exists exactly while a live DLL does; a module still
    // mapped without it is a stale leftover that would swallow the injection.
    if (!DllChannel.IsLoaded()) {
      foreach (int pid in EdgeProcesses.PidsWithModule(Paths.SplitDllName)) {
        Injector.FreeModule(pid, Paths.SplitDllName);
      }
    }
    // After the stale copy is freed: a mapped DLL holds its file open, so the
    // fresh one cannot be written over it until then.
    Payload.Unpack();
    int[] missing = EdgeProcesses.PidsMissingModule(Paths.SplitDllName)
                        .Where(pid => EdgeWatch.BuildOf(pid) == EdgeBuild.Pinned)
                        .Where(pid => reach == Reach.EveryBrowser || holders.Contains(pid))
                        .ToArray();
    if (missing.Length > 0) {
      Injector.InjectInto(missing, Paths.SplitDll);
    }
    DllChannel.SignalConfigChanged();
  }

  // A loaded DLL that is not this build's is ejected so Apply injects the fresh
  // one: a rebuilt panel takes effect with no Edge restart. With auto-inject off
  // a DLL already loaded is still replaced, but an Edge without one waits for
  // the Inject button.
  public static void SyncOnStart() {
    if (EdgeInstall.IsSimulated) {
      return;
    }
    lock (Settings.Gate) {
      Settings settings = Settings.Load();
      bool isEnabled = IsWanted(settings);
      if (!isEnabled) {
        return;
      }
      // Taken before a stale DLL unloads, so its processes get the fresh one.
      int[] holders = EdgeProcesses.PidsWithModule(Paths.SplitDllName);
      bool isStale = DllChannel.IsLoaded() && !Payload.IsUnpackedCurrent();
      if (isStale) {
        DllChannel.SignalUnload();
        WaitForUnload();
      }
      Reach reach = settings.AutoInject ? Reach.EveryBrowser : Reach.Holders;
      Apply(settings, reach, holders);
    }
  }

  // For a caller that reports the outcome (the Inject button, auto-inject):
  // returns once the DLL is live, or after a bounded wait when it never comes
  // up. DllMain creates the unload event before LoadLibrary returns, so the wait
  // is normally over at the first poll.
  public static void ApplyAndWaitForLoad(Settings settings) {
    Apply(settings, Reach.EveryBrowser, []);
    bool isLoadExpected = !EdgeInstall.IsSimulated && IsWanted(settings)
        && PinnedBrowserPids().Length > 0;
    if (isLoadExpected) {
      WaitForLoad();
    }
  }

  // Whether the settings want the DLL in Edge at all.
  public static bool IsWanted(Settings settings) =>
      Effective(settings).NeedsDll;

  private static int[] PinnedBrowserPids() =>
      EdgeProcesses.BrowserPids().Where(pid => EdgeWatch.BuildOf(pid) == EdgeBuild.Pinned).ToArray();

  private static void WaitForLoad() {
    for (int i = 0; i < LoadPolls; i++) {
      if (DllChannel.IsLoaded()) {
        return;
      }
      Thread.Sleep(LoadPollMilliseconds);
    }
  }

  private static void WaitForUnload() {
    for (int i = 0; i < UnloadPolls; i++) {
      bool isGone = EdgeProcesses.PidsWithModule(Paths.SplitDllName).Length == 0;
      if (isGone) {
        return;
      }
      Thread.Sleep(UnloadPollMilliseconds);
    }
  }

  // A config file that cannot be written (a full disk, a locked folder) still
  // lets the load or unload go ahead; the DLL keeps the last state it read.
  private static void WriteConfigIfPossible(Settings settings, Features effective) {
    try {
      DllChannel.WriteConfig(DllConfig.From(settings, effective));
    } catch (System.IO.IOException) {
    } catch (System.UnauthorizedAccessException) {
    }
  }

  private static Features Effective(Settings settings) =>
      EdgeSupport.Effective(settings.Features, EdgeWatch.CurrentBuild(), EdgeBuild.Pinned);
}
