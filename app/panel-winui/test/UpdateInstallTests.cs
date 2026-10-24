using System;
using System.IO;
using System.Security.Cryptography;
using System.Text;

namespace EdgePlus;

// Plain files under a throwaway temp folder stand in for the exes.
internal static class UpdateInstallTests {
  public static void Run() {
    string temp = Path.Combine(Path.GetTempPath(), "edgeplus-update-" + Guid.NewGuid().ToString("N"));
    Directory.CreateDirectory(temp);
    try {
      ChecksADigest(temp);
      SwapsTheExeAndKeepsTheOldOneAside(temp);
      RefusesAWrongDigestAndChangesNothing(temp);
      LeavesTheRunningExeAloneWhenStagingFails(temp);
      DeletesWhatTheSwapLeft(temp);
    } finally {
      Directory.Delete(temp, recursive: true);
    }
  }

  private static string Digest(string contents) =>
      Convert.ToHexString(SHA256.HashData(Encoding.UTF8.GetBytes(contents)));

  private static string Write(string folder, string name, string contents) {
    Directory.CreateDirectory(folder);
    string path = Path.Combine(folder, name);
    File.WriteAllText(path, contents);
    return path;
  }

  private static void ChecksADigest(string temp) {
    string file = Write(temp, "digest.exe", "new build");
    Expect.That(UpdateInstall.IsDigestOf(file, Digest("new build")), "the digest of the contents");
    Expect.That(UpdateInstall.IsDigestOf(file, Digest("new build").ToLowerInvariant()),
                "in either case");
    Expect.That(!UpdateInstall.IsDigestOf(file, Digest("another build")), "not another's");
  }

  private static void SwapsTheExeAndKeepsTheOldOneAside(string temp) {
    string folder = Path.Combine(temp, "swap");
    string running = Write(folder, "edgeplus.exe", "old build");
    string downloaded = Write(Path.Combine(temp, "swap-update"), "EdgePlus-0.0.2.exe", "new build");
    DateTime now = new(2026, 10, 24, 12, 0, 0, DateTimeKind.Utc);

    UpdateInstall.Install(downloaded, running, Digest("new build"), now);

    Expect.Equal("new build", File.ReadAllText(running), "the new build has the exe's name");
    Expect.Equal("old build", File.ReadAllText(UpdateInstall.RetiredName(running, now)),
                 "the old one waits beside it under a retired name");
    Expect.That(!File.Exists(running + ".new"), "nothing is left staged");
    Expect.That(File.Exists(downloaded), "the download is left for its owner to clear");
  }

  private static void RefusesAWrongDigestAndChangesNothing(string temp) {
    string folder = Path.Combine(temp, "wrong");
    string running = Write(folder, "edgeplus.exe", "old build");
    string downloaded = Write(Path.Combine(temp, "wrong-update"), "EdgePlus-0.0.2.exe", "tampered");
    bool isRefused = false;
    try {
      UpdateInstall.Install(downloaded, running, Digest("new build"), DateTime.UtcNow);
    } catch (InvalidDataException) {
      isRefused = true;
    }
    Expect.That(isRefused, "a download that is not what the manifest named is refused");
    Expect.Equal("old build", File.ReadAllText(running), "the running exe is as it was");
    Expect.Equal(1, Directory.GetFiles(folder).Length, "and nothing else is left beside it");
  }

  private static void LeavesTheRunningExeAloneWhenStagingFails(string temp) {
    string folder = Path.Combine(temp, "fail");
    string running = Write(folder, "edgeplus.exe", "old build");
    string downloaded = Write(Path.Combine(temp, "fail-update"), "EdgePlus-0.0.2.exe", "new build");
    // A directory where the staged copy goes makes the copy itself fail.
    Directory.CreateDirectory(running + ".new");
    bool isFailed = false;
    try {
      UpdateInstall.Install(downloaded, running, Digest("new build"), DateTime.UtcNow);
    } catch (Exception error) when (error is IOException or UnauthorizedAccessException) {
      isFailed = true;
    }
    Expect.That(isFailed, "a swap that cannot stage its copy fails");
    Expect.Equal("old build", File.ReadAllText(running), "with the running exe untouched");
  }

  private static void DeletesWhatTheSwapLeft(string temp) {
    string folder = Path.Combine(temp, "clean");
    string running = Write(folder, "edgeplus.exe", "current");
    string retired = Write(folder, "edgeplus.exe.old-123", "old");
    string staged = Write(folder, "edgeplus.exe.new", "staged");
    string other = Write(folder, "settings.json", "keep");

    UpdateInstall.DeleteLeftovers(running);

    Expect.That(!File.Exists(retired) && !File.Exists(staged), "retired and staged copies go");
    Expect.That(File.Exists(running) && File.Exists(other), "the exe and other files stay");
    UpdateInstall.DeleteLeftovers(running);
    Expect.That(File.Exists(running), "running it again is harmless");
  }
}
