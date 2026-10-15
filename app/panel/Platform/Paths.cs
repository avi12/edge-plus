using System;
using System.IO;

namespace FixEdge;

// The files the panel keeps under its data folder: the user's settings and the
// native split DLL it injects into Edge.
internal static class Paths {
  public static string Root {
    get;
  } = Path.Combine(
      Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "FixEdge");

  public static string Settings { get; } = Path.Combine(Root, "settings.json");

  // The native split DLL, unpacked here for the panel to inject into Edge.
  public static string SplitDll { get; } = Path.Combine(Root, "fixedge-split.dll");

  public static string ReadSettings() => File.Exists(Settings) ? File.ReadAllText(Settings) : "{}";

  // Written whole then swapped in, so a watcher never reads half a file.
  public static void WriteAtomically(string path, string contents) {
    Directory.CreateDirectory(Path.GetDirectoryName(path)!);
    string temporary = path + ".tmp";
    File.WriteAllText(temporary, contents);
    File.Move(temporary, path, overwrite: true);
  }
}
