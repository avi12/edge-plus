using System;
using System.IO;
using System.Security.Cryptography;

namespace EdgePlus;

// Swapping a downloaded exe in for the running one. A running exe cannot be
// overwritten but can be renamed, so the new one takes its name and the old one
// waits beside it under a retired name until the next start deletes it.
internal static class UpdateInstall {
  private const string StagedSuffix = ".new";
  private const string RetiredMarker = ".old-";

  public static bool IsDigestOf(string path, string sha256) {
    using FileStream stream = File.OpenRead(path);
    string actual = Convert.ToHexString(SHA256.HashData(stream));
    return string.Equals(actual, sha256, StringComparison.OrdinalIgnoreCase);
  }

  // The new exe is copied beside the running one first, so the swap itself is a
  // rename on one volume: a copy cut short never leaves a half exe under the
  // running exe's name. Throws if it cannot (a folder Edge+ may not write to),
  // with the running exe back under its own name.
  public static void Install(string downloaded, string running, string sha256, DateTime nowUtc) {
    string staged = running + StagedSuffix;
    File.Copy(downloaded, staged, overwrite: true);
    if (!IsDigestOf(staged, sha256)) {
      File.Delete(staged);
      throw new InvalidDataException("The staged update does not match its digest.");
    }
    string retired = RetiredName(running, nowUtc);
    File.Move(running, retired);
    try {
      File.Move(staged, running);
    } catch (Exception error) when (error is IOException or UnauthorizedAccessException) {
      File.Move(retired, running);
      throw;
    }
  }

  public static string RetiredName(string running, DateTime nowUtc) =>
      $"{running}{RetiredMarker}{nowUtc.Ticks}";

  // After a restart, the exe that was swapped out is no longer running.
  public static void DeleteLeftovers(string running) {
    string? folder = Path.GetDirectoryName(running);
    if (folder is null) {
      return;
    }
    string name = Path.GetFileName(running);
    string[] patterns = [$"{name}{RetiredMarker}*", name + StagedSuffix];
    foreach (string pattern in patterns) {
      try {
        foreach (string file in Directory.EnumerateFiles(folder, pattern)) {
          File.Delete(file);
        }
      } catch (IOException) {
      } catch (UnauthorizedAccessException) {
      }
    }
  }
}
