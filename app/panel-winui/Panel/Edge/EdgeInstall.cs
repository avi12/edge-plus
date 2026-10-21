using System;
using System.IO;
using Microsoft.Win32;

namespace EdgePlus;

// The installed Edge, as files on disk, read-only: where it lives and which
// build it is. Edge installs an update into a new version folder and swaps its
// launcher (msedge.exe) on the next restart, so the launcher's file version is
// the build Edge runs now, or will run when it next starts — cheap to read, and
// readable while Edge is closed.
//
// A dev seam: with EDGEPLUS_SIMULATE_EDGE_DIR set, that folder is taken as the
// Edge install and running Edge processes are not consulted, so an update can
// be simulated (a folder holding any versioned msedge.exe) without touching
// the real Edge. The panel then also never injects or writes the DLL's config
// (DllSync), so a simulation cannot change the Edge that is running.
internal static class EdgeInstall {
  public const string SimulatedDirectoryVariable = "EDGEPLUS_SIMULATE_EDGE_DIR";
  public const string LauncherName = "msedge.exe";
  // Edge's own registration of its launcher: the key's default value is its path.
  private const string AppPathsKey =
      @"SOFTWARE\Microsoft\Windows\CurrentVersion\App Paths\msedge.exe";

  public static string? SimulatedDirectory {
    get {
      string? folder = Environment.GetEnvironmentVariable(SimulatedDirectoryVariable);
      return string.IsNullOrWhiteSpace(folder) ? null : folder;
    }
  }

  public static bool IsSimulated => SimulatedDirectory is not null;

  // The folder msedge.exe is in, or null when Edge is not installed.
  public static string? ApplicationDirectory {
    get {
      if (SimulatedDirectory is string simulated) {
        return simulated;
      }
      string? launcher = Launcher(Registry.LocalMachine) ?? Launcher(Registry.CurrentUser);
      return launcher is null ? null : Path.GetDirectoryName(launcher);
    }
  }

  // The build of the msedge.exe in `applicationDirectory`, or null when there is
  // none or it cannot be read.
  public static FileVersion? InstalledBuild(string? applicationDirectory) =>
      applicationDirectory is null
          ? null
          : FileVersion.Of(Path.Combine(applicationDirectory, LauncherName));

  private static string? Launcher(RegistryKey hive) {
    try {
      using RegistryKey? key = hive.OpenSubKey(AppPathsKey);
      return key?.GetValue(null) is string path && File.Exists(path) ? path : null;
    } catch (UnauthorizedAccessException) {
      return null;
    } catch (System.Security.SecurityException) {
      return null;
    }
  }
}
