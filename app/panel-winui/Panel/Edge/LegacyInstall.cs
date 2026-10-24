using System;
using System.IO;
using System.Threading;

namespace EdgePlus;

// What an install from before the rename leaves behind (the project was called
// fix-edge): its split DLL, still loaded in Edge and listening on its own
// unload event, and its data folder. These names are the old ones on purpose;
// every current name is Edge+'s. Runs before anything is injected or read, so
// the old DLL is gone before the new one comes in (two would hook the same
// code twice) and the settings are in place before the window reads them.
internal static class LegacyInstall {
  private const string DataFolderName = "FixEdge";
  private const string DllName = "fixedge-split.dll";
  // Files the old install left that nothing reads any more.
  private static readonly string[] s_unusedFiles = [DllName, "fixedge-config.bin"];
  private const string UnloadEvent = @"Local\fixedge-split-unload";
  private const int UnloadPolls = 40;
  private const int UnloadPollMilliseconds = 100;

  public static void Run() {
    if (EdgeInstall.IsSimulated) {
      return;
    }
    EjectDll();
    CarryData();
  }

  // A process the old DLL is still hooked in is left alone: injecting the new
  // one there would hook it twice. It clears when that Edge restarts.
  public static bool IsDllLoadedIn(int pid) => EdgeProcesses.ModulePath(pid, DllName) is not null;

  private static void EjectDll() {
    SignalUnload();
    for (int i = 0; i < UnloadPolls; i++) {
      if (EdgeProcesses.PidsWithModule(DllName).Length == 0) {
        return;
      }
      Thread.Sleep(UnloadPollMilliseconds);
    }
    // The event exists exactly while the DLL is live. Without it a module still
    // mapped is a leftover whose hooks are gone, which only needs freeing.
    if (IsUnloadEventOpen()) {
      PanelLog.Write("the pre-rename split DLL did not unload; it stays until Edge restarts");
      return;
    }
    foreach (int pid in EdgeProcesses.PidsWithModule(DllName)) {
      Injector.FreeModule(pid, DllName);
    }
  }

  private static void SignalUnload() {
    if (!EventWaitHandle.TryOpenExisting(UnloadEvent, out EventWaitHandle? unload)) {
      return;
    }
    using (unload) {
      unload.Set();
    }
  }

  private static bool IsUnloadEventOpen() {
    bool isOpen = EventWaitHandle.TryOpenExisting(UnloadEvent, out EventWaitHandle? unload);
    unload?.Dispose();
    return isOpen;
  }

  private static void CarryData() {
    string legacyRoot = Path.Combine(
        Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), DataFolderName);
    bool isCarried = LegacyData.Carry(legacyRoot, Paths.DataRoot, Path.GetFileName(Paths.Settings),
                                      s_unusedFiles);
    if (isCarried) {
      PanelLog.Write($"data carried over from {legacyRoot}");
    }
  }
}
