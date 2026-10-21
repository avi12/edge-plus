using System;
using System.IO;

namespace EdgePlus;

// Where the panel assembly comes from, and the decisions about it that need no
// WinUI and no loader: whether this is a dev run, what a loaded copy is called,
// whether a build is new. Pure, so it is unit tested.
internal static class PanelSource {
  public const string CopyPrefix = "edgeplus-panel-";
  public const string AssemblyExtension = ".dll";
  public const string SymbolsExtension = ".pdb";

  // A dev run watches the panel's build output and swaps each rebuild in. The
  // rule: the running exe sits inside the repo checkout it was built from, and
  // that checkout's panel build output exists. A published edgeplus.exe copied
  // anywhere else always runs the panel it carries.
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

  // The copy a load is made from, never the build output itself: a loaded
  // assembly holds its file, and the next build must be free to overwrite the
  // output. Named by the time it was taken, so every swap gets a fresh file
  // and a copy still held by an older context is never written over.
  public static string CopyName(DateTime takenUtc) =>
      $"{CopyPrefix}{takenUtc.Ticks}{AssemblyExtension}";

  // Whether a copy file name is one of ours (and so ours to clean up).
  public static bool IsCopy(string fileName) =>
      fileName.StartsWith(CopyPrefix, StringComparison.OrdinalIgnoreCase)
      && (fileName.EndsWith(AssemblyExtension, StringComparison.OrdinalIgnoreCase)
          || fileName.EndsWith(SymbolsExtension, StringComparison.OrdinalIgnoreCase));

  // Whether `candidate` is a different build from the one loaded, by content:
  // a rebuild that changed nothing (an up-to-date build touching the file)
  // swaps nothing.
  public static bool IsNewBuild(ReadOnlySpan<byte> loaded, ReadOnlySpan<byte> candidate) =>
      candidate.Length > 0 && !candidate.SequenceEqual(loaded);
}
