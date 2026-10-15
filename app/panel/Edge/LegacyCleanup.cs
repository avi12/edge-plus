using System;
using System.IO;
using Microsoft.Win32;

namespace FixEdge;

// Removes what earlier versions installed for the extension-based architecture:
// the loaded extension folder, the native-messaging host exe and its manifest,
// the status file, and Edge's registry pointer to the host. Best-effort and
// idempotent — a fresh install simply finds nothing to remove.
internal static class LegacyCleanup {
  private const string HostName = "com.avi.fix_edge";
  private const string EdgeHostsKey = @"Software\Microsoft\Edge\NativeMessagingHosts\";

  public static void Run() {
    DeleteDirectory(Path.Combine(Paths.Root, "extension"));
    foreach (string name in
             new[] { "native-host.json", "status.json", "extension.stamp" }) {
      DeleteFile(Path.Combine(Paths.Root, name));
    }
    DeleteHostExecutables();
    DeleteRegistryKey();
  }

  // The host was swapped in by renaming the running copy aside, so several
  // "fixedge-host.exe[.ticks].retired" may remain beside the live one.
  private static void DeleteHostExecutables() {
    if (!Directory.Exists(Paths.Root)) {
      return;
    }
    try {
      foreach (string file in Directory.EnumerateFiles(Paths.Root, "fixedge-host.exe*")) {
        DeleteFile(file);
      }
    } catch (IOException) {
    } catch (UnauthorizedAccessException) {
    }
  }

  private static void DeleteRegistryKey() {
    try {
      Registry.CurrentUser.DeleteSubKeyTree(EdgeHostsKey + HostName, throwOnMissingSubKey: false);
    } catch (UnauthorizedAccessException) {
    }
  }

  private static void DeleteDirectory(string path) {
    try {
      if (Directory.Exists(path)) {
        Directory.Delete(path, recursive: true);
      }
    } catch (IOException) {
    } catch (UnauthorizedAccessException) {
    }
  }

  private static void DeleteFile(string path) {
    try {
      if (File.Exists(path)) {
        File.Delete(path);
      }
    } catch (IOException) {
    } catch (UnauthorizedAccessException) {
    }
  }
}
