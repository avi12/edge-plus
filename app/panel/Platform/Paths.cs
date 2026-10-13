using System;
using System.IO;

namespace FixEdge;

// Every file the panel, the host and the extension hand to each other. The
// panel writes settings, the host relays them to Edge and writes status back;
// neither process talks to the other directly.
internal static class Paths {
  public static string Root { get; } = Path.Combine(
      Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData), "FixEdge");

  // The folder Edge's "Load unpacked" points at.
  public static string Extension { get; } = Path.Combine(Root, "extension");

  // Rewritten whenever the unpacked extension changes; the host watches it
  // and tells the running extension to reload itself.
  public static string ExtensionStamp { get; } = Path.Combine(Root, "extension.stamp");

  public static string Settings { get; } = Path.Combine(Root, "settings.json");

  // Where the native-messaging host is unpacked, outside the app's own folder
  // so the app can be rebuilt or moved while Edge keeps the host running.
  public static string HostExecutable { get; } = Path.Combine(Root, "fixedge-host.exe");

  public static string Status { get; } = Path.Combine(Root, "status.json");

  // The native split DLL, unpacked here for the panel to inject into Edge.
  public static string SplitDll { get; } = Path.Combine(Root, "fixedge-split.dll");

  public static string HostManifest { get; } = Path.Combine(Root, "native-host.json");

  public static string ReadSettings() => File.Exists(Settings) ? File.ReadAllText(Settings) : "{}";

  // Written whole then swapped in, so a watcher never reads half a file.
  public static void WriteAtomically(string path, string contents) {
    Directory.CreateDirectory(Path.GetDirectoryName(path)!);
    string temporary = path + ".tmp";
    File.WriteAllText(temporary, contents);
    File.Move(temporary, path, overwrite: true);
  }
}
