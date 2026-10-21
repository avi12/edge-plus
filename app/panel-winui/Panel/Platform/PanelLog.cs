using System;
using System.IO;

namespace EdgePlus;

// The panel's log, beside its settings: where a failure's details go, so the
// window only ever says a plain sentence (Wording) and never a raw exception
// message.
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
