using System;
using System.Runtime.InteropServices;

namespace EdgePlus;

// Asks Windows to relaunch the process after an update replaces its package.
// Windows only does so for a process that has run for at least 60 seconds.
internal static class AppRestart {
  [Flags]
  private enum RestartFlags : uint {
    NoCrash = 1,
    NoHang = 2,
    // The startup task already covers a reboot.
    NoReboot = 8,
  }

  public static void RegisterForUpdates(string commandLine) {
    int hr = RegisterApplicationRestart(commandLine,
                                        RestartFlags.NoCrash | RestartFlags.NoHang | RestartFlags.NoReboot);
    if (hr != 0) {
      Marshal.ThrowExceptionForHR(hr);
    }
  }

  [DllImport("kernel32.dll", CharSet = CharSet.Unicode)]
  private static extern int RegisterApplicationRestart(string commandLine, RestartFlags flags);
}
