using System;
using System.Collections.Generic;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace FixEdge;

// Where a theme in the library came from, so the installer knows how to add it
// to Edge and the panel knows how to label it.
internal enum ThemeSource {
  // A theme from the Chrome Web Store or Edge Add-ons, installed from the store.
  Store,
  // A local theme .crx the user picked.
  Local,
}

// One theme the user added to their library: an installed (or installing) Edge
// theme extension, identified by its extension id.
internal sealed record ThemeEntry(string Id, string Name, ThemeSource Source, string Origin);

// The theming choice the user keeps: the library of themes they added, and which
// one to use in each Windows mode (an empty id means the default theme). Stored
// under "theming" in settings.json, beside "features"; the switch itself is a
// FeatureSettings flag (FeatureId.Theming).
internal sealed class ThemingConfig {
  public List<ThemeEntry> Themes { get; init; } = new();
  public string LightThemeId { get; set; } = "";
  public string DarkThemeId { get; set; } = "";
}

internal static class ThemeLibrary {
  private const string ThemingKey = "theming";
  private const string ThemesKey = "themes";
  private const string LightKey = "light";
  private const string DarkKey = "dark";
  private const string IdKey = "id";
  private const string NameKey = "name";
  private const string SourceKey = "source";
  private const string OriginKey = "origin";

  private static readonly JsonSerializerOptions s_indented = new() { WriteIndented = true };

  public static ThemingConfig Load() {
    var theming = Settings()[ThemingKey] as JsonObject;
    if (theming is null) {
      return new ThemingConfig();
    }
    var config = new ThemingConfig {
      LightThemeId = theming[LightKey]?.GetValue<string>() ?? "",
      DarkThemeId = theming[DarkKey]?.GetValue<string>() ?? "",
    };
    if (theming[ThemesKey] is JsonArray themes) {
      foreach (JsonNode? node in themes) {
        if (node is JsonObject theme && theme[IdKey]?.GetValue<string>() is { } id) {
          config.Themes.Add(new ThemeEntry(
              id, theme[NameKey]?.GetValue<string>() ?? id,
              ParseSource(theme[SourceKey]?.GetValue<string>()),
              theme[OriginKey]?.GetValue<string>() ?? ""));
        }
      }
    }
    return config;
  }

  public static void Save(ThemingConfig config) {
    var themes = new JsonArray();
    foreach (ThemeEntry theme in config.Themes) {
      themes.Add(new JsonObject {
        [IdKey] = theme.Id,
        [NameKey] = theme.Name,
        [SourceKey] = theme.Source.ToString().ToLowerInvariant(),
        [OriginKey] = theme.Origin,
      });
    }
    JsonObject settings = Settings();
    settings[ThemingKey] = new JsonObject {
      [ThemesKey] = themes,
      [LightKey] = config.LightThemeId,
      [DarkKey] = config.DarkThemeId,
    };
    Paths.WriteAtomically(Paths.Settings, settings.ToJsonString(s_indented));
  }

  private static ThemeSource ParseSource(string? source) =>
      string.Equals(source, "local", StringComparison.OrdinalIgnoreCase)
          ? ThemeSource.Local
          : ThemeSource.Store;

  private static JsonObject Settings() {
    try {
      return JsonNode.Parse(Paths.ReadSettings()) as JsonObject ?? new JsonObject();
    } catch (JsonException) {
      return new JsonObject();
    }
  }
}
