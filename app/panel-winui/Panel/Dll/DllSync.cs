using System.Linq;
using System.Threading;

namespace EdgePlus;

// The one place that brings the running Edge in line with what the user wants:
// write the config the DLL reads, load or unload the DLL, then signal a live
// re-read. The DLL only ever goes into a browser process running the build it
// was made for, and is sent the features as they apply on the current build
// (EdgeSupport.Effective): a paused feature goes as off, so on an unsupported
// build nothing is injected and a loaded copy unloads. In a simulation
// (EdgeInstall) nothing here runs, so the Edge that is running is never
// touched.
internal static class DllSync {
  private const int UnloadPolls = 40;
  private const int UnloadPollMilliseconds = 100;

  public static void Apply(Settings settings) {
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
                        .ToArray();
    if (missing.Length > 0) {
      Injector.InjectInto(missing, Paths.SplitDll);
    }
    DllChannel.SignalConfigChanged();
  }

  // At startup: if the DLL loaded in Edge is not this build's, eject it and let
  // Apply inject the fresh one, so a rebuilt panel takes effect with no Edge
  // restart.
  public static void SyncOnStart() {
    if (EdgeInstall.IsSimulated) {
      return;
    }
    lock (Settings.Gate) {
      Settings settings = Settings.Load();
      bool isEnabled = Effective(settings).NeedsDll;
      bool isStale = DllChannel.IsLoaded() && !Payload.IsUnpackedCurrent();
      if (isEnabled && isStale) {
        DllChannel.SignalUnload();
        WaitForUnload();
      }
      if (isEnabled) {
        Apply(settings);
      }
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
