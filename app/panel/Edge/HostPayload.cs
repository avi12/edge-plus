using System;
using System.IO;
using System.Linq;

namespace FixEdge;

// Puts the native-messaging host where Edge is told to find it. Edge keeps the
// host running while the extension is open, and a running exe cannot be
// overwritten - but it can be renamed. So a newer host moves the running one
// aside and takes its name; Edge starts the new one the next time the
// extension connects, which the extension's reload makes immediate.
internal static class HostPayload {
  private const string Resource = "fixedge-host.exe";
  private const string RetiredSuffix = ".retired";

  // True when a newer host was put in place, which only reaches Edge once
  // the extension reconnects.
  public static bool Unpack() {
    DeleteRetired();
    using var stream = typeof(HostPayload).Assembly.GetManifestResourceStream(Resource)
        ?? throw new InvalidOperationException($"fixedge.exe carries no {Resource}");
    using var buffer = new MemoryStream();
    stream.CopyTo(buffer);
    byte[] contents = buffer.ToArray();

    bool isCurrent = File.Exists(Paths.HostExecutable) &&
                     File.ReadAllBytes(Paths.HostExecutable).AsSpan().SequenceEqual(contents);
    if (isCurrent) {
      return false;
    }
    if (File.Exists(Paths.HostExecutable)) {
      File.Move(
          Paths.HostExecutable, $"{Paths.HostExecutable}.{DateTime.UtcNow.Ticks}{RetiredSuffix}");
    }
    Directory.CreateDirectory(Paths.Root);
    File.WriteAllBytes(Paths.HostExecutable, contents);
    return true;
  }

  // A retired host still running holds its file; it goes on a later launch.
  private static void DeleteRetired() {
    if (!Directory.Exists(Paths.Root)) {
      return;
    }
    foreach (string retired in Directory.EnumerateFiles(Paths.Root, "*" + RetiredSuffix).ToList()) {
      try {
        File.Delete(retired);
      } catch (Exception exception) when (exception is IOException or UnauthorizedAccessException) {
      }
    }
  }
}
