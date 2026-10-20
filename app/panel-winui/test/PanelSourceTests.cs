using System;
using System.IO;

namespace EdgePlus;

internal static class PanelSourceTests {
  public static void Run() {
    string repository = Path.Combine(Path.GetTempPath(), $"edgeplus-repo-{Environment.ProcessId}");
    string output = Path.Combine(repository, "app", "panel-winui", "Panel", "bin",
                                 "edgeplus-panel.dll");
    Directory.CreateDirectory(Path.GetDirectoryName(output)!);
    File.WriteAllBytes(output, [1]);
    string hostBin = Path.Combine(repository, "app", "panel-winui", "bin");
    try {
      Expect.That(PanelSource.IsDevRun(hostBin, repository, output),
                  "an exe inside its repo, with the panel built, is a dev run");
      Expect.That(PanelSource.IsDevRun(hostBin, repository + Path.DirectorySeparatorChar, output),
                  "a trailing separator on the root changes nothing");
      Expect.That(!PanelSource.IsDevRun(@"C:\Program Files\Edge+", repository, output),
                  "an exe outside the repo runs the panel it carries");
      Expect.That(!PanelSource.IsDevRun(repository + "-other", repository, output),
                  "a sibling folder sharing the root's prefix is not inside it");
      Expect.That(!PanelSource.IsDevRun(hostBin, null, output), "no recorded repo, no dev run");
      File.Delete(output);
      Expect.That(!PanelSource.IsDevRun(hostBin, repository, output),
                  "no panel build output, no dev run");
    } finally {
      Directory.Delete(repository, recursive: true);
    }

    var taken = new DateTime(2026, 10, 21, 1, 2, 3, DateTimeKind.Utc);
    string copy = PanelSource.CopyName(taken);
    Expect.Equal($"edgeplus-panel-{taken.Ticks}.dll", copy, "a copy is named by its time");
    Expect.That(PanelSource.CopyName(taken.AddTicks(1)) != copy, "every swap gets a new file");
    Expect.That(PanelSource.IsCopy(copy), "a copy is ours to clean up");
    Expect.That(PanelSource.IsCopy("edgeplus-panel-1.pdb"), "so are its symbols");
    Expect.That(!PanelSource.IsCopy("edgeplus-panel.dll"), "the build output is not a copy");
    Expect.That(!PanelSource.IsCopy("swap.log"), "nor is the log");

    Expect.That(PanelSource.IsNewBuild([1, 2], [1, 3]), "different bytes are a new build");
    Expect.That(!PanelSource.IsNewBuild([1, 2], [1, 2]), "the same bytes are not");
    Expect.That(!PanelSource.IsNewBuild([1, 2], []), "an empty read is not a build");
  }
}
