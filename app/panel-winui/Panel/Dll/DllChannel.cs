using System.Threading;

namespace EdgePlus;

// The DLL holds the unload event open for its whole lifetime, so the event
// existing is exactly "the DLL is loaded right now": the panel never tracks
// per-process state. Both event names are the DLL's own (dllmain.cpp).
internal static class DllChannel {
  // Atomic, so the DLL never reads half a file.
  public static void WriteConfig(DllConfig config) =>
      Paths.WriteAtomically(Paths.ConfigFile, ConfigWire.Encode(config));

  public static bool IsLoaded() {
    bool isOpen = EventWaitHandle.TryOpenExisting(Shared.UnloadEvent, out EventWaitHandle? unload);
    unload?.Dispose();
    return isOpen;
  }

  // Both signals are no-ops when no DLL is loaded.
  public static void SignalUnload() => Signal(Shared.UnloadEvent);

  public static void SignalConfigChanged() => Signal(Shared.ConfigEvent);

  private static void Signal(string eventName) {
    if (!EventWaitHandle.TryOpenExisting(eventName, out EventWaitHandle? handle)) {
      return;
    }
    using (handle) {
      handle.Set();
    }
  }
}
