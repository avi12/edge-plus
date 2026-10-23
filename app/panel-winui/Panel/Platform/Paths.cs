using System;
using System.IO;

namespace EdgePlus;

// The data folder is a contract with the DLL (config.h, log.h), so its location
// comes from the DLL's own constant (Shared). The earlier Tauri panel used the
// same folder and files, so an install of it carries over.
internal static class Paths {
  private const string SettingsFileName = "settings.json";
  private const string SplitDllFileName = "fixedge-split.dll";
  private const string ThemesFolderName = "themes";
  private const string ThemeKeysFolderName = "keys";
  private const string TemporarySuffix = ".tmp";
  private const string PausedToastFileName = "paused-toast-build.txt";

  // The desired state the DLL reads at load and on every change.
  public static string ConfigFile {
    get;
  } =
      Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData)
      + Shared.ConfigFileRelativePath;

  public static string DataRoot { get; } = Path.GetDirectoryName(ConfigFile)!;

  public static string Settings { get; } = Path.Combine(DataRoot, SettingsFileName);

  // Unpacked here for injection into Edge.
  public static string SplitDll { get; } = Path.Combine(DataRoot, SplitDllFileName);

  public static string SplitDllName => SplitDllFileName;

  // The build the paused-features toast last spoke of, so it is raised once per build.
  public static string PausedToastBuild { get; } = Path.Combine(DataRoot, PausedToastFileName);

  // Local theme .crx files are copied here to install by path.
  public static string ThemesRoot { get; } = Path.Combine(DataRoot, ThemesFolderName);

  // Kept so a theme packed from a folder or .zip and added again keeps its
  // extension id.
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
