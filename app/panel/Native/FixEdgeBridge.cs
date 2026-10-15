using System;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;

namespace FixEdge;

// The live link from the panel to the injected DLL. Both features — the split
// and the theming — ride one DLL, so this owns the shared decisions: the DLL is
// loaded while either feature is on, and the DLL's behaviour (which features are
// active, which theme per OS mode) is pushed over WM_COPYDATA so changes take
// effect with no restart. The wire struct mirrors config.h field-for-field.
internal static class FixEdgeBridge {
  // Must equal config::kMessageId ("FDEX") in app/native-split/src/config.h.
  private const uint ConfigMessageId = 0x58454446;
  // Must equal config::kIdCapacity.
  private const int IdCapacity = 64;

  // Bring the running Edge in line with what the user wants: record the desired
  // state to disk (so a DLL injected now or later reads it), load or unload the
  // DLL, then push the state live to any already-running DLL.
  public static void Sync(bool splitEnabled, bool themingEnabled, ThemingConfig theming) {
    byte[] payload = BuildPayload(splitEnabled, themingEnabled, theming);
    WriteConfigFile(payload);
    bool shouldBeLoaded = splitEnabled || themingEnabled;
    if (!shouldBeLoaded) {
      NativeSplit.Disable();
      return;
    }
    NativeSplit.Enable(Paths.SplitDll);
    foreach (Process browser in EdgeBrowserProcesses()) {
      SendCopyData(browser.MainWindowHandle, payload);
    }
  }

  private static void WriteConfigFile(byte[] payload) {
    try {
      Directory.CreateDirectory(Paths.Root);
      string temporary = Paths.ConfigFile + ".tmp";
      File.WriteAllBytes(temporary, payload);
      File.Move(temporary, Paths.ConfigFile, overwrite: true);
    } catch (IOException) {
      // A running DLL still gets the state over WM_COPYDATA below.
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

  private static void SendCopyData(IntPtr window, byte[] payload) {
    if (window == IntPtr.Zero) {
      return;
    }
    IntPtr buffer = Marshal.AllocHGlobal(payload.Length);
    try {
      Marshal.Copy(payload, 0, buffer, payload.Length);
      var data = new CopyDataStruct {
        DwData = (IntPtr)ConfigMessageId,
        CbData = payload.Length,
        LpData = buffer,
      };
      SendMessageTimeout(window, WmCopyData, IntPtr.Zero, ref data,
                         SmtoAbortIfHung, TimeoutMilliseconds, out _);
    } finally {
      Marshal.FreeHGlobal(buffer);
    }
  }

  private static Process[] EdgeBrowserProcesses() =>
      Process.GetProcessesByName(EdgeBrowser.ExecutableName)
          .Where(process => process.MainWindowHandle != IntPtr.Zero)
          .ToArray();

  private const uint WmCopyData = 0x004A;
  private const uint SmtoAbortIfHung = 0x0002;
  private const uint TimeoutMilliseconds = 2000;

  [StructLayout(LayoutKind.Sequential)]
  private struct CopyDataStruct {
    public IntPtr DwData;
    public int CbData;
    public IntPtr LpData;
  }

  [DllImport("user32.dll", SetLastError = true)]
  private static extern IntPtr SendMessageTimeout(
      IntPtr window, uint message, IntPtr wParam, ref CopyDataStruct lParam, uint flags,
      uint timeoutMilliseconds, out IntPtr result);
}
