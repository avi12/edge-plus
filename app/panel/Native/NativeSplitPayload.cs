using System;
using System.IO;

namespace FixEdge;

// Carries fixedge-split.dll inside the panel exe and unpacks it next to the host,
// so the switch always injects the DLL that matches this build. A DLL currently
// loaded in Edge holds its file open; unpacking then keeps the loaded copy (the
// toggle re-injects the fresh one after the next eject) rather than failing.
internal static class NativeSplitPayload {
  private const string Resource = "fixedge-split.dll";

  public static void Unpack() {
    using Stream? stream = typeof(NativeSplitPayload).Assembly.GetManifestResourceStream(Resource);
    if (stream is null) {
      return;  // a build without the native DLL simply has no native-split switch
    }
    using var buffer = new MemoryStream();
    stream.CopyTo(buffer);
    byte[] contents = buffer.ToArray();

    bool isCurrent = File.Exists(Paths.SplitDll) &&
                     File.ReadAllBytes(Paths.SplitDll).AsSpan().SequenceEqual(contents);
    if (isCurrent) {
      return;
    }
    Directory.CreateDirectory(Paths.Root);
    try {
      File.WriteAllBytes(Paths.SplitDll, contents);
    } catch (IOException) {
      // Locked because it is loaded in Edge right now; the current copy stays.
    }
  }

  public static bool Exists =>
      typeof(NativeSplitPayload).Assembly.GetManifestResourceStream(Resource) is not null;
}
