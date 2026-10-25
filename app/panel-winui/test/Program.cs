using System;

namespace EdgePlus;

// A failed check ends its suite (as in app\native-split\test), because the
// checks after it read results it vouched for.
internal static class Program {
  private static readonly (string Name, Action Run)[] s_suites = [
    ("switches", SwitchesTests.Run),
    ("settings", SettingsTests.Run),
    ("config wire", ConfigWireTests.Run),
    ("edge build", EdgeBuildTests.Run),
    ("theme packing", ThemePackTests.Run),
    ("installed themes", InstalledThemesTests.Run),
    ("dev-mode prompt", DevModePromptTests.Run),
    ("startup entry", StartupEntryTests.Run),
    ("wording", WordingTests.Run),
    ("resize handles", ResizeHandlesTests.Run),
    ("keyed list", KeyedListTests.Run),
    ("panel source", PanelSourceTests.Run),
    ("theme order", ThemeOrderTests.Run),
    ("edge support", EdgeSupportTests.Run),
    ("panel layout", PanelLayoutTests.Run),
    ("inject offer", InjectOfferTests.Run),
    ("legacy data", LegacyDataTests.Run),
    ("update feed", UpdateFeedTests.Run),
    ("update install", UpdateInstallTests.Run),
    ("msi install", MsiInstallTests.Run),
  ];

  private static int Main() {
    int failedSuites = 0;
    foreach ((string name, Action run) in s_suites) {
      int checksBefore = Expect.Count;
      try {
        run();
        Console.WriteLine($"[{name}] {Expect.Count - checksBefore} checks passed");
      } catch (CheckFailedException failure) {
        failedSuites++;
        Console.WriteLine($"[{name}] FAIL: {failure.Message}");
      }
    }
    bool isClean = failedSuites == 0;
    Console.WriteLine(isClean ? "all suites passed" : $"{failedSuites} suite(s) failed");
    return isClean ? 0 : 1;
  }
}

internal sealed class CheckFailedException(string what) : Exception(what);

internal static class Expect {
  public static int Count {
    get; private set;
  }

  public static void That(bool isTrue, string what) {
    Count++;
    if (!isTrue) {
      throw new CheckFailedException(what);
    }
  }

  public static void Equal<T>(T expected, T actual, string what) {
    bool isEqual = Equals(expected, actual);
    That(isEqual, $"{what}: expected <{expected}>, got <{actual}>");
  }
}
