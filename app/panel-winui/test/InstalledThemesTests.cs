using System;
using System.IO;
using System.Text.Json.Nodes;

namespace EdgePlus;

internal static class InstalledThemesTests {
  public static void Run() {
    NamesComeOnlyFromThemes();
    LocalisedNamesResolveIgnoringKeyCase();
    TheActiveThemeAndTheRegisteredIdsAreRead();
    VersionsCompareNumerically();
    AScanListsInstalledThemesAndMarksTheActiveOne();
  }

  private static JsonNode Json(string text) => JsonNode.Parse(text)!;

  private static void NamesComeOnlyFromThemes() {
    Expect.Equal("Dusk", InstalledThemes.ThemeName(Json("""{"name":"Dusk","theme":{}}"""), null),
                 "a theme's plain name");
    Expect.That(InstalledThemes.ThemeName(Json("""{"name":"uBlock"}"""), null) is null,
                "an extension without a theme key is not a theme");
  }

  private static void LocalisedNamesResolveIgnoringKeyCase() {
    JsonNode messages = Json("""{"APPNAME":{"message":"Sunset"}}""");
    Expect.Equal("Sunset",
                 InstalledThemes.ThemeName(Json("""{"name":"__MSG_appName__","theme":{}}"""),
                                           messages),
                 "a __MSG_ name resolves");
    Expect.Equal("__MSG_gone__", InstalledThemes.ResolveMessage("__MSG_gone__", null),
                 "no messages keeps the placeholder");
    Expect.Equal("__MSG_gone__",
                 InstalledThemes.ResolveMessage("__MSG_gone__", Json("""{"x":{"message":"y"}}""")),
                 "an unknown key keeps the placeholder");
  }

  private static void TheActiveThemeAndTheRegisteredIdsAreRead() {
    Expect.Equal("abc",
                 InstalledThemes.ActiveThemeId(Json("""{"extensions":{"theme":{"id":"abc"}}}""")),
                 "the active id");
    Expect.That(InstalledThemes.ActiveThemeId(Json("""{"extensions":{}}""")) is null,
                "no theme in use");
    var ids = InstalledThemes.RegisteredIds(Json("""{"extensions":{"settings":{"a":{},"b":{}}}}"""));
    Expect.That(ids is { Count: 2 } && ids.Contains("a") && ids.Contains("b"), "both ids");
    Expect.That(InstalledThemes.RegisteredIds(Json("{}")) is null, "unknown is not empty");
  }

  private static void VersionsCompareNumerically() {
    long[] older = InstalledThemes.VersionKey(@"x\1.9.0_0");
    long[] newer = InstalledThemes.VersionKey(@"x\1.10.0_1");
    Expect.That(older[1] == 9 && newer[1] == 10, "1.10 is ten, not one-dot-one-oh");
  }

  private static void AScanListsInstalledThemesAndMarksTheActiveOne() {
    string root = Path.Combine(Path.GetTempPath(), $"edgeplus-themes-{Environment.ProcessId}");
    if (Directory.Exists(root)) {
      Directory.Delete(root, recursive: true);
    }
    string profile = Path.Combine(root, "Default");
    string id = new('a', 32);
    string other = new('b', 32);
    string version = Path.Combine(profile, "Extensions", id, "1.0_0");
    string locale = Path.Combine(version, "_locales", "en");
    Directory.CreateDirectory(locale);
    // With a byte-order mark, as Edge writes some of these.
    File.WriteAllText(Path.Combine(version, "manifest.json"),
                      """{"name":"__MSG_n__","default_locale":"en","theme":{}}""",
                      new System.Text.UTF8Encoding(encoderShouldEmitUTF8Identifier: true));
    File.WriteAllText(Path.Combine(locale, "messages.json"), """{"n":{"message":"Night"}}""");
    string plain = Path.Combine(profile, "Extensions", other, "2.0_0");
    Directory.CreateDirectory(plain);
    File.WriteAllText(Path.Combine(plain, "manifest.json"), """{"name":"Tool"}""");
    File.WriteAllText(Path.Combine(profile, "Preferences"),
                      """{"extensions":{"theme":{"id":""" + $"\"{id}\"" + "}}}");

    var themes = InstalledThemes.Scan(root);
    Directory.Delete(root, recursive: true);
    Expect.Equal(1, themes.Count, "only the theme is listed");
    Expect.Equal(new InstalledTheme(id, "Night", IsActive: true), themes[0],
                 "named from its locale and marked in use");
  }
}
