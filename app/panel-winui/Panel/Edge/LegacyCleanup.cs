using System;
using System.IO;
using Microsoft.Win32;

namespace EdgePlus;

// Removes what versions before 2026-10-15 installed for the extension-based
// architecture: the loaded extension folder, the native-messaging host exe and
// its manifest, the status file, and Edge's registry pointer to the host.
// Best-effort and idempotent — a fresh install simply finds nothing.
internal static class LegacyCleanup {
  private const string HostName = "com.avi.fix_edge";
  private const string EdgeHostsKey = @"Software\Microsoft\Edge\NativeMessagingHosts\";
  private const string ExtensionFolder = "extension";
  // The host was swapped in by renaming the running copy aside, so several
  // "fixedge-host.exe[.ticks].retired" may remain beside the live one.
  private const string HostExecutablePattern = "fixedge-host.exe*";
  private static readonly string[] s_files = ["native-host.json", "status.json", "extension.stamp"];

  public static void Run() {
    DeleteDirectory(Path.Combine(Paths.DataRoot, ExtensionFolder));
    foreach (string name in s_files) {
      DeleteFile(Path.Combine(Paths.DataRoot, name));
    }
    DeleteHostExecutables();
    try {
      Registry.CurrentUser.DeleteSubKeyTree(EdgeHostsKey + HostName, throwOnMissingSubKey: false);
    } catch (UnauthorizedAccessException) {
    }
  }

  private static void DeleteHostExecutables() {
    if (!Directory.Exists(Paths.DataRoot)) {
      return;
    }
    try {
      foreach (string file in Directory.EnumerateFiles(Paths.DataRoot, HostExecutablePattern)) {
        DeleteFile(file);
      }
    } catch (IOException) {
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
