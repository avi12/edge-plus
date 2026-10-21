using System;
using System.IO;

namespace EdgePlus;

// The files the panel reads and writes, in the one place that names them.
//
// The data folder is a shared contract with the injected DLL, which reads the
// config file from it (config.h) and writes its log beside it (log.h); its
// location comes from the DLL's own constant (Shared), so the two cannot name
// different folders. The earlier Tauri panel used the same folder and files,
// so an install of it carries over.
internal static class Paths {
  private const string SettingsFileName = "settings.json";
  private const string SplitDllFileName = "fixedge-split.dll";
  private const string ThemesFolderName = "themes";
  private const string ThemeKeysFolderName = "keys";
  private const string TemporarySuffix = ".tmp";
  private const string PausedToastFileName = "paused-toast-build.txt";

  // %LOCALAPPDATA%\FixEdge\fixedge-config.bin: the desired state the DLL reads
  // at load and on every change.
  public static string ConfigFile {
    get;
  } =
      Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData)
      + Shared.ConfigFileRelativePath;

  // %LOCALAPPDATA%\FixEdge, the panel's data folder.
  public static string DataRoot { get; } = Path.GetDirectoryName(ConfigFile)!;

  // The user's switches and theme library.
  public static string Settings { get; } = Path.Combine(DataRoot, SettingsFileName);

  // The native-split DLL, unpacked here for injection into Edge.
  public static string SplitDll { get; } = Path.Combine(DataRoot, SplitDllFileName);

  public static string SplitDllName => SplitDllFileName;

  // The Edge build the paused-features toast last spoke of, so it is raised
  // once per build.
  public static string PausedToastBuild { get; } = Path.Combine(DataRoot, PausedToastFileName);

  // Local theme .crx files are copied here to install by path.
  public static string ThemesRoot { get; } = Path.Combine(DataRoot, ThemesFolderName);

  // The signing keys of themes packed from a folder or .zip, so a theme added
  // again keeps its extension id.
  public static string ThemeKeys { get; } = Path.Combine(ThemesRoot, ThemeKeysFolderName);

  // Written whole then swapped in, so a reader (the DLL, Edge, the other
  // panel) never sees half a file.
  public static void WriteAtomically(string path, byte[] contents) {
    Directory.CreateDirectory(Path.GetDirectoryName(path)!);
    string temporary = path + TemporarySuffix;
    File.WriteAllBytes(temporary, contents);
    File.Move(temporary, path, overwrite: true);
  }

  public static void WriteAtomically(string path, string contents) =>
      WriteAtomically(path, System.Text.Encoding.UTF8.GetBytes(contents));
}
