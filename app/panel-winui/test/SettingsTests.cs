using System;
using System.IO;

namespace EdgePlus;

internal static class SettingsTests {
  public static void Run() {
    DefaultsAreSplitOnTheRestOff();
    AFileFromTheGeminiDaysStillLoads();
    AnExistingFileLoads();
    WritesTheExistingShape();
    RoundTripsThroughAFile();
    BrokenOrNullSectionsFallBack();
    RemovingAThemeClearsItsAssignments();
    TheSwitchesMapOneToOne();
    ReadingModeIsOnlyItsSwitch();
    AutoInjectDefaultsOnAndIsSavedCamelCase();
    AutoUpdateDefaultsOnAndIsSavedCamelCase();
  }

  // A file from before the switch existed checks for updates.
  private static void AutoUpdateDefaultsOnAndIsSavedCamelCase() {
    Expect.That(new Settings().AutoUpdate, "auto-update defaults on");
    Expect.That(Settings.Parse("""{"autoInject":true}""").AutoUpdate,
                "a file without autoUpdate loads with it on");
    Expect.That(!Settings.Parse("""{"autoUpdate":false}""").AutoUpdate, "autoUpdate off is read");
    string json = new Settings { AutoUpdate = false }.ToJson();
    Expect.That(json.Contains("\"autoUpdate\": false", StringComparison.Ordinal),
                "it is written camelCase");
    Expect.That(!Settings.Parse(json).AutoUpdate, "off survives a round trip");
  }

  // A file from before the switch existed keeps the old behaviour: inject.
  private static void AutoInjectDefaultsOnAndIsSavedCamelCase() {
    Expect.That(new Settings().AutoInject, "auto-inject defaults on");
    Expect.That(Settings.Parse("""{"features":{"nativeSplit":true}}""").AutoInject,
                "a file without autoInject loads with it on");
    Expect.That(!Settings.Parse("""{"autoInject":false}""").AutoInject, "autoInject off is read");
    var settings = new Settings { AutoInject = false };
    string json = settings.ToJson();
    Expect.That(json.Contains("\"autoInject\": false", StringComparison.Ordinal),
                "it is written camelCase");
    Expect.That(!Settings.Parse(json).AutoInject, "off survives a round trip");
  }

  // The reading themes are now picked in reading mode itself, so the old
  // "readingMode" object is read past and never written.
  private static void ReadingModeIsOnlyItsSwitch() {
    Expect.That(!new Settings().Features.ReadingTheme, "reading mode is off by default");
    Settings loaded = Settings.Parse(
        """{"features":{"readingTheme":true},"readingMode":{"light":"skyBlue","dark":"darkGrey"}}""");
    Expect.That(loaded.Features.ReadingTheme, "the switch is read beside an old readingMode");
    string json = loaded.ToJson();
    Expect.That(json.Contains("\"readingTheme\": true", StringComparison.Ordinal),
                "the switch is written");
    Expect.That(!json.Contains("readingMode", StringComparison.Ordinal),
                "the old readingMode object is not written");
  }

  private static void DefaultsAreSplitOnTheRestOff() {
    var settings = new Settings();
    Expect.That(settings.Features.NativeSplit, "the split defaults on");
    foreach (Feature feature in Enum.GetValues<Feature>()) {
      bool isExpectedOn = feature is Feature.NativeSplit or Feature.TabShape;
      Expect.Equal(isExpectedOn, settings.Features.IsOn(feature), $"default of {feature}");
    }
    Expect.That(settings.Features.NeedsDll, "the split alone needs the DLL");
  }

  private static void AFileFromTheGeminiDaysStillLoads() {
    const string text =
        """{"features":{"nativeSplit":false,"assistant":"gemini","enhancedFind":true}}""";
    Settings settings = Settings.Parse(text);
    Expect.That(!settings.Features.NativeSplit, "nativeSplit off is read");
    Expect.That(settings.Features.EnhancedFind, "enhancedFind on is read");
    Expect.Equal("", settings.Theming.Light, "a missing theming section is empty");
  }

  // The shape the earlier Tauri panel wrote.
  private static void AnExistingFileLoads() {
    const string text = """
        {
          "features": { "nativeSplit": true, "theming": true, "quietDevMode": false,
                        "protocolHandlers": true, "enhancedFind": false,
                        "webUiFollowsTheme": true, "livePreview": false },
          "theming": {
            "themes": [ { "id": "abc", "name": "Dusk", "source": "store", "origin": "https://x" },
                        { "id": "def", "name": "Mine", "source": "local", "origin": "C:\\t" } ],
            "light": "abc", "dark": "def"
          },
          "lastEdgeBuild": "155.0.4283.62"
        }
        """;
    Settings settings = Settings.Parse(text);
    Expect.That(settings.Features.Theming && settings.Features.ProtocolHandlers,
                "the switches are read");
    Expect.Equal(2, settings.Theming.Themes.Count, "both themes are read");
    Expect.Equal(ThemeSource.Store, settings.Theming.Themes[0].Source, "store is an enum");
    Expect.Equal(ThemeSource.Local, settings.Theming.Themes[1].Source, "local is an enum");
    Expect.Equal(@"C:\t", settings.Theming.Themes[1].Origin, "the origin is read");
    Expect.Equal("def", settings.Theming.ThemeFor(ColorMode.Dark), "the dark pick is read");
    Expect.Equal("155.0.4283.62", settings.LastEdgeBuild, "the last build is read");
  }

  private static void WritesTheExistingShape() {
    var settings = new Settings();
    settings.Theming.AddOrReplace(new ThemeEntry {
      Id = "abc",
      Name = "Dusk",
      Source = ThemeSource.Local,
      Origin = "o",
    });
    string json = settings.ToJson();
    foreach (string key in new[] { "\"features\"", "\"nativeSplit\"", "\"webUiFollowsTheme\"",
                                   "\"livePreview\"", "\"theming\"", "\"themes\"",
                                   "\"source\": \"local\"", "\"lastEdgeBuild\"" }) {
      Expect.That(json.Contains(key, StringComparison.Ordinal), $"the file has {key}");
    }
    Expect.That(!json.Contains("needsDll", StringComparison.OrdinalIgnoreCase),
                "the derived switch is not stored");
  }

  private static void RoundTripsThroughAFile() {
    string path = Path.Combine(Path.GetTempPath(), $"edgeplus-settings-{Environment.ProcessId}.json");
    var settings = new Settings();
    settings.Features.Set(Feature.LivePreview, true);
    settings.Theming.Assign(ColorMode.Light, "abc");
    settings.Save(path);
    Settings loaded = Settings.Load(path);
    File.Delete(path);
    Expect.That(loaded.Features.LivePreview, "a switch survives the file");
    Expect.Equal("abc", loaded.Theming.Light, "an assignment survives the file");
    Expect.That(!File.Exists(path + ".tmp"), "the temporary file was swapped in");
    Expect.That(Settings.Load(path).Features.NativeSplit, "a missing file is the defaults");
  }

  private static void BrokenOrNullSectionsFallBack() {
    Expect.That(Settings.Parse("not json").Features.NativeSplit, "garbage is the defaults");
    Settings nulls = Settings.Parse("""{"features":null,"theming":null}""");
    Expect.That(nulls.Features.NativeSplit, "a null features section is the defaults");
    Expect.Equal(0, nulls.Theming.Themes.Count, "a null theming section is empty");
  }

  private static void RemovingAThemeClearsItsAssignments() {
    var theming = new Theming();
    theming.AddOrReplace(new ThemeEntry { Id = "a", Name = "A" });
    theming.AddOrReplace(new ThemeEntry { Id = "a", Name = "A2" });
    Expect.Equal(1, theming.Themes.Count, "adding the same id replaces it");
    Expect.Equal("A2", theming.Themes[0].Name, "the replacement wins");
    theming.Assign(ColorMode.Light, "a");
    theming.Assign(ColorMode.Dark, "a");
    theming.Remove("a");
    Expect.That(theming.Light == "" && theming.Dark == "", "both assignments fall back");
    Expect.Equal(0, theming.Themes.Count, "the theme is gone");
  }

  private static void TheSwitchesMapOneToOne() {
    foreach (Feature feature in Enum.GetValues<Feature>()) {
      var features = new Features { NativeSplit = false, TabShape = false };
      features.Set(feature, true);
      foreach (Feature other in Enum.GetValues<Feature>()) {
        Expect.Equal(other == feature, features.IsOn(other), $"setting {feature} reads {other}");
      }
      bool isPanelOwn = feature == Feature.QuietDevMode;
      Expect.Equal(!isPanelOwn, features.NeedsDll, $"{feature} needs the DLL");
    }
  }
}
