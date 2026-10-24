using System;
using System.IO;

namespace EdgePlus;

// Folders under a throwaway temp root, never the real data folder.
internal static class LegacyDataTests {
  private const string Settings = "settings.json";
  private const string SkippedDll = "fixedge-split.dll";
  private const string SkippedConfig = "fixedge-config.bin";
  private static readonly string[] s_skipped = [SkippedDll, SkippedConfig];

  public static void Run() {
    string temp = Path.Combine(Path.GetTempPath(), "edgeplus-legacy-data-" + Guid.NewGuid().ToString("N"));
    try {
      CarriesAnOldInstall(temp);
      LeavesANewInstallAlone(temp);
      DoesNothingWithoutAnOldFolder(temp);
    } finally {
      Directory.Delete(temp, recursive: true);
    }
  }

  private static void CarriesAnOldInstall(string temp) {
    string legacy = Path.Combine(temp, "a-old");
    string root = Path.Combine(temp, "a-new");
    Write(legacy, Settings, "{\"features\":1}");
    Write(legacy, "reading-themes.bin", "bin");
    Write(legacy, SkippedDll, "dll");
    Write(legacy, SkippedConfig, "cfg");
    Write(Path.Combine(legacy, "themes"), "dusk.crx", "crx");

    bool isCarried = LegacyData.Carry(legacy, root, Settings, s_skipped);

    Expect.That(isCarried, "an old install is carried over");
    Expect.Equal("{\"features\":1}", File.ReadAllText(Path.Combine(root, Settings)), "the settings come across");
    Expect.That(File.Exists(Path.Combine(root, "reading-themes.bin")), "its other files too");
    Expect.That(File.Exists(Path.Combine(root, "themes", "dusk.crx")), "and its folders");
    Expect.That(!File.Exists(Path.Combine(root, SkippedDll)), "but not the old DLL");
    Expect.That(!File.Exists(Path.Combine(root, SkippedConfig)), "nor its unused config file");
    Expect.That(!Directory.Exists(legacy), "the old folder is removed once copied");
    Expect.That(!LegacyData.Carry(legacy, root, Settings, s_skipped), "a second run has nothing to do");
  }

  private static void LeavesANewInstallAlone(string temp) {
    string legacy = Path.Combine(temp, "b-old");
    string root = Path.Combine(temp, "b-new");
    Write(legacy, Settings, "old");
    Write(root, Settings, "new");

    bool isCarried = LegacyData.Carry(legacy, root, Settings, s_skipped);

    Expect.That(!isCarried, "a folder with settings of its own is not carried into");
    Expect.Equal("new", File.ReadAllText(Path.Combine(root, Settings)), "its settings are kept");
    Expect.That(Directory.Exists(legacy), "and the old folder is left as it was");
  }

  private static void DoesNothingWithoutAnOldFolder(string temp) {
    string root = Path.Combine(temp, "c-new");

    bool isCarried = LegacyData.Carry(Path.Combine(temp, "c-old"), root, Settings, s_skipped);

    Expect.That(!isCarried && !Directory.Exists(root), "no old folder, no new one");
  }

  private static void Write(string folder, string name, string contents) {
    Directory.CreateDirectory(folder);
    File.WriteAllText(Path.Combine(folder, name), contents);
  }
}
