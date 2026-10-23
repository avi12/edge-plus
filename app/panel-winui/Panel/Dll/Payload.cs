using System;
using System.IO;

namespace EdgePlus;

// The split DLL embedded in the panel, unpacked to the data folder so the
// injected DLL always matches this build. A copy loaded in Edge holds its file
// open: unpacking then keeps it, and the next inject after an eject picks up
// the fresh one.
internal static class Payload {
  // Must match the LogicalName in EdgePlus.csproj.
  private const string ResourceName = "payload/fixedge-split.dll";

  private static readonly Lazy<byte[]> s_dll = new(ReadEmbedded);

  public static bool IsUnpackedCurrent() {
    try {
      return File.ReadAllBytes(Paths.SplitDll).AsSpan().SequenceEqual(s_dll.Value);
    } catch (IOException) {
      return false;
    } catch (UnauthorizedAccessException) {
      return false;
    }
  }

  public static void Unpack() {
    if (IsUnpackedCurrent()) {
      return;
    }
    try {
      Directory.CreateDirectory(Paths.DataRoot);
      File.WriteAllBytes(Paths.SplitDll, s_dll.Value);
    } catch (IOException) {
    } catch (UnauthorizedAccessException) {
    }
  }

  private static byte[] ReadEmbedded() {
    using Stream stream = typeof(Payload).Assembly.GetManifestResourceStream(ResourceName)
        ?? throw new InvalidOperationException($"The build did not embed {ResourceName}");
    using var copy = new MemoryStream();
    stream.CopyTo(copy);
    return copy.ToArray();
  }
}
