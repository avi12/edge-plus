using System;
using System.IO;

namespace EdgePlus;

// The native-split DLL carried inside the panel exe (an embedded resource, see
// the csproj), unpacked beside the panel's data so the switch always injects
// the DLL that matches this build. A copy currently loaded in Edge holds its
// file open; unpacking then keeps the loaded copy rather than failing, and the
// next inject after an eject picks up the fresh one.
internal static class Payload {
  // Must match the LogicalName in EdgePlus.csproj.
  private const string ResourceName = "payload/fixedge-split.dll";

  private static readonly Lazy<byte[]> s_dll = new(ReadEmbedded);

  // Whether the unpacked DLL is byte-identical to the one this build carries.
  public static bool IsUnpackedCurrent() {
    try {
      return File.ReadAllBytes(Paths.SplitDll).AsSpan().SequenceEqual(s_dll.Value);
    } catch (IOException) {
      return false;
    } catch (UnauthorizedAccessException) {
      return false;
    }
  }

  // Write the carried DLL to its data-folder path unless an identical copy is
  // already there. A locked file (the DLL is loaded) keeps the loaded copy.
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
