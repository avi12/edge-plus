using System;
using System.IO;
using System.Text;
using System.Threading;

namespace FixEdge;

// The live link from the panel to the injected DLL. Both features — the split
// and the theming — ride one DLL, so this owns the shared decisions: the DLL is
// loaded while either feature is on, and the DLL's behaviour (which features are
// active, which theme per OS mode) is driven by a config file the DLL reads.
//
// The desired state is written to that file, then a named event is signalled so
// the loaded DLL re-reads and applies it live (no restart). The DLL posts the
// apply to its own window, so the panel never has to know which window the DLL
// hooked. The wire bytes mirror config.h field-for-field.
internal static class FixEdgeBridge {
  // Must match config::kIdCapacity in app/native-split/src/config.h.
  private const int IdCapacity = 64;
  // Must match kConfigEvent in app/native-split/src/dllmain.cpp.
  private const string ConfigEventName = @"Local\fixedge-config-changed";

  // Bring the running Edge in line with what the user wants: record the desired
  // state to disk (so a DLL injected now or later reads it), load or unload the
  // DLL, then signal a loaded DLL to re-read and apply it live.
  public static void Sync(bool splitEnabled, bool themingEnabled, ThemingConfig theming) {
    WriteConfigFile(BuildPayload(splitEnabled, themingEnabled, theming));
    bool shouldBeLoaded = splitEnabled || themingEnabled;
    if (!shouldBeLoaded) {
      NativeSplit.Disable();
      return;
    }
    NativeSplit.Enable(Paths.SplitDll);
    SignalConfigChanged();
  }

  private static void WriteConfigFile(byte[] payload) {
    try {
      Directory.CreateDirectory(Paths.Root);
      string temporary = Paths.ConfigFile + ".tmp";
      File.WriteAllBytes(temporary, payload);
      File.Move(temporary, Paths.ConfigFile, overwrite: true);
    } catch (IOException) {
      // The DLL will read whatever is there now; the next Sync retries.
    }
  }

  private static void SignalConfigChanged() {
    try {
      using EventWaitHandle changed = EventWaitHandle.OpenExisting(ConfigEventName);
      changed.Set();
    } catch (WaitHandleCannotBeOpenedException) {
      // No DLL is loaded to listen; it reads the file when it next loads.
    }
  }

  private static byte[] BuildPayload(bool splitEnabled, bool themingEnabled, ThemingConfig theming) {
    byte[] lightId = Encoding.ASCII.GetBytes(theming.LightThemeId);
    byte[] darkId = Encoding.ASCII.GetBytes(theming.DarkThemeId);
    var wire = new byte[4 * sizeof(uint) + 2 * IdCapacity];
    BitConverter.GetBytes(splitEnabled ? 1u : 0u).CopyTo(wire, 0);
    BitConverter.GetBytes(themingEnabled ? 1u : 0u).CopyTo(wire, 4);
    BitConverter.GetBytes((uint)lightId.Length).CopyTo(wire, 8);
    BitConverter.GetBytes((uint)darkId.Length).CopyTo(wire, 12);
    Array.Copy(lightId, 0, wire, 16, Math.Min(lightId.Length, IdCapacity));
    Array.Copy(darkId, 0, wire, 16 + IdCapacity, Math.Min(darkId.Length, IdCapacity));
    return wire;
  }
}
