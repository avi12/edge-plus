using System;
using System.IO;

namespace EdgePlus;

// PanelLoader's pure decisions, split out to be unit tested.
internal static class PanelSource {
  public const string CopyPrefix = "edgeplus-panel-";
  public const string AssemblyExtension = ".dll";
  public const string SymbolsExtension = ".pdb";

  // A published edgeplus.exe copied out of the checkout always runs the panel
  // it carries.
  public static bool IsDevRun(string exeDirectory, string? repositoryRoot,
                              string? panelBuildOutput) {
    if (repositoryRoot is null || panelBuildOutput is null) {
      return false;
    }
    string root = Path.TrimEndingDirectorySeparator(Path.GetFullPath(repositoryRoot))
        + Path.DirectorySeparatorChar;
    bool isInsideRepository = Path.GetFullPath(exeDirectory)
        .StartsWith(root, StringComparison.OrdinalIgnoreCase);
    return isInsideRepository && File.Exists(panelBuildOutput);
  }

  // A load is made from a copy, never the build output: a loaded assembly locks
  // its file, and the next build must be free to overwrite it. A fresh name per
  // swap never writes over a copy an older context still holds.
  public static string CopyName(DateTime takenUtc) =>
      $"{CopyPrefix}{takenUtc.Ticks}{AssemblyExtension}";

  public static bool IsCopy(string fileName) =>
      fileName.StartsWith(CopyPrefix, StringComparison.OrdinalIgnoreCase)
      && (fileName.EndsWith(AssemblyExtension, StringComparison.OrdinalIgnoreCase)
          || fileName.EndsWith(SymbolsExtension, StringComparison.OrdinalIgnoreCase));

  // By content: an up-to-date build that only touches the file swaps nothing.
  public static bool IsNewBuild(ReadOnlySpan<byte> loaded, ReadOnlySpan<byte> candidate) =>
      candidate.Length > 0 && !candidate.SequenceEqual(loaded);
}
