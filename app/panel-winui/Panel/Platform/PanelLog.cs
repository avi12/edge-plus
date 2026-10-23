using System;
using System.IO;

namespace EdgePlus;

// Where a failure's details go, so the window only ever shows a plain sentence
// (Wording), never a raw exception message.
internal static class PanelLog {
  private const string FileName = "panel.log";

  public static string Path { get; } = System.IO.Path.Combine(Paths.DataRoot, FileName);

  public static void Write(string what, Exception error) => Write($"{what}: {error}");

  public static void Write(string line) {
    try {
      Directory.CreateDirectory(Paths.DataRoot);
      File.AppendAllText(Path, $"{DateTime.Now:O} {line}{Environment.NewLine}");
    } catch (IOException) {
    } catch (UnauthorizedAccessException) {
    }
  }
}
