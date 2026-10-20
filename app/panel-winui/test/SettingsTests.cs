using System;
using System.IO;

namespace EdgePlus;

internal static class SettingsTests {
  public static void Run() {
    DefaultsAreSplitOnTheRestOff();
    AFileFromTheGeminiDaysStillLoads();
    TheTauriPanelsFileLoads();
    WritesTheTauriShape();
    RoundTripsThroughAFile();
    BrokenOrNullSectionsFallBack();
    RemovingAThemeClearsItsAssignments();
    TheSwitchesMapOneToOne();
  }

  private static void DefaultsAreSplitOnTheRestOff() {
    var settings = new Settings();
    Expect.That(settings.Features.NativeSplit, "the split defaults on");
    foreach (Feature feature in Enum.GetValues<Feature>()) {
      bool isExpectedOn = feature == Feature.NativeSplit;
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

  private static void TheTauriPanelsFileLoads() {
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

  private static void WritesTheTauriShape() {
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
      var features = new Features { NativeSplit = false };
      features.Set(feature, true);
      foreach (Feature other in Enum.GetValues<Feature>()) {
        Expect.Equal(other == feature, features.IsOn(other), $"setting {feature} reads {other}");
      }
      bool isPanelOwn = feature == Feature.QuietDevMode;
      Expect.Equal(!isPanelOwn, features.NeedsDll, $"{feature} needs the DLL");
    }
  }
}
