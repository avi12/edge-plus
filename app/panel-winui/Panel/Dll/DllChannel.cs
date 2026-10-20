using System.Threading;

namespace EdgePlus;

// The DLL's config channel and its off switch.
//
// The panel records the desired state to the config file and signals the
// config-changed event; the loaded DLL re-reads the file and applies it live.
// The unload event is held open by the DLL for its whole lifetime, so it
// existing is exactly "the DLL is loaded right now" — the panel never tracks
// per-process state. Both names are the DLL's own (dllmain.cpp, via Shared).
internal static class DllChannel {
  // Write the desired state the DLL reads, atomically, so a read never sees
  // half a file.
  public static void WriteConfig(DllConfig config) =>
      Paths.WriteAtomically(Paths.ConfigFile, ConfigWire.Encode(config));

  // Whether the DLL is loaded in any Edge process right now.
  public static bool IsLoaded() {
    bool isOpen = EventWaitHandle.TryOpenExisting(Shared.UnloadEvent, out EventWaitHandle? unload);
    unload?.Dispose();
    return isOpen;
  }

  // Ask the DLL to remove its hooks and unload itself (a no-op if none is
  // loaded).
  public static void SignalUnload() => Signal(Shared.UnloadEvent);

  // Ask a loaded DLL to re-read the config file and apply it live (a no-op if
  // none is loaded to receive it).
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
