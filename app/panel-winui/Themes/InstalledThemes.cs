using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace EdgePlus;

// A theme Edge has installed in one of its profiles; IsActive when Edge is
// showing it right now.
internal sealed record InstalledTheme(string Id, string Name, bool IsActive);

// The themes Edge already has installed (from Edge Add-ons or the Chrome Web
// Store), read straight from its profiles on disk, read-only. A theme is an
// extension whose manifest has a "theme" key; the DLL applies it by id exactly
// like one the panel installed, so nothing here writes anything. The Tauri
// panel's installed_themes.rs.
internal static class InstalledThemes {
  private const string MessagePrefix = "__MSG_";
  private const string MessageSuffix = "__";
  private const string FallbackLocale = "en";
  private const string ExtensionsFolder = "Extensions";
  private const string LocalesFolder = "_locales";
  private const string ManifestFile = "manifest.json";
  private const string MessagesFile = "messages.json";
  private const string PreferencesFile = "Preferences";
  private const string SecurePreferencesFile = "Secure Preferences";

  // The name of the theme a manifest describes, or null when it is not a theme.
  // `messages` is its resolved messages.json, for a __MSG_key__ name.
  public static string? ThemeName(JsonNode? manifest, JsonNode? messages) {
    bool isTheme = manifest?["theme"] is JsonObject;
    string? name = AsString(manifest?["name"]);
    if (!isTheme || name is null) {
      return null;
    }
    return ResolveMessage(name, messages);
  }

  // A __MSG_key__ placeholder looked up in `messages` (keys matched ignoring
  // case, as Chromium does); anything else is returned as is.
  public static string ResolveMessage(string text, JsonNode? messages) {
    bool isPlaceholder = text.StartsWith(MessagePrefix, StringComparison.Ordinal)
        && text.EndsWith(MessageSuffix, StringComparison.Ordinal)
        && text.Length >= MessagePrefix.Length + MessageSuffix.Length;
    if (!isPlaceholder || messages is not JsonObject entries) {
      return text;
    }
    string key = text[MessagePrefix.Length..^MessageSuffix.Length];
    foreach ((string name, JsonNode? entry) in entries) {
      bool isKey = string.Equals(name, key, StringComparison.OrdinalIgnoreCase);
      string? message = isKey ? AsString(entry?["message"]) : null;
      if (message is not null) {
        return message;
      }
    }
    return text;
  }

  // The id of the theme a profile's Preferences says is in use.
  public static string? ActiveThemeId(JsonNode? preferences) =>
      AsString(preferences?["extensions"]?["theme"]?["id"]);

  // A JSON string's text, or null for anything else.
  private static string? AsString(JsonNode? node) =>
      node is JsonValue value && value.GetValueKind() == JsonValueKind.String
          ? value.GetValue<string>()
          : null;

  // The extension ids a profile's Secure Preferences still registers, so a
  // folder left behind by an uninstall is not listed; null when unknown.
  public static HashSet<string>? RegisteredIds(JsonNode? securePreferences) =>
      securePreferences?["extensions"]?["settings"] is JsonObject settings
          ? settings.Select(entry => entry.Key).ToHashSet()
          : null;

  // Every installed theme across all profiles under `userData`, each id once
  // (active if it is active in any profile), sorted by name.
  public static List<InstalledTheme> Scan(string userData) {
    var found = new List<InstalledTheme>();
    foreach (string profile in ProfileFolders(userData)) {
      foreach (InstalledTheme theme in ScanProfile(profile)) {
        int known = found.FindIndex(existing => existing.Id == theme.Id);
        if (known < 0) {
          found.Add(theme);
        } else if (theme.IsActive) {
          found[known] = found[known] with {
            IsActive = true
          };
        }
      }
    }
    return found.OrderBy(theme => theme.Name.ToLowerInvariant(), StringComparer.Ordinal).ToList();
  }

  // Default, Profile 1, ...: every folder with an Extensions folder.
  private static IEnumerable<string> ProfileFolders(string userData) {
    try {
      return Directory.GetDirectories(userData)
          .Where(folder => Directory.Exists(Path.Combine(folder, ExtensionsFolder)))
          .ToArray();
    } catch (IOException) {
      return [];
    } catch (UnauthorizedAccessException) {
      return [];
    }
  }

  private static List<InstalledTheme> ScanProfile(string profile) {
    string? active = ActiveThemeId(EdgeUserData.ReadJson(Path.Combine(profile, PreferencesFile)));
    HashSet<string>? registered =
        RegisteredIds(EdgeUserData.ReadJson(Path.Combine(profile, SecurePreferencesFile)));
    var themes = new List<InstalledTheme>();
    string[] extensions;
    try {
      extensions = Directory.GetDirectories(Path.Combine(profile, ExtensionsFolder));
    } catch (IOException) {
      return themes;
    }
    foreach (string extension in extensions) {
      string id = Path.GetFileName(extension);
      bool isRegistered = registered is null || registered.Contains(id);
      if (!ExtensionId.IsValid(id) || !isRegistered) {
        continue;
      }
      string? version = NewestVersionFolder(extension);
      string? name = version is null ? null : ThemeNameIn(version);
      if (name is null) {
        continue;
      }
      themes.Add(new InstalledTheme(id, name, IsActive: active == id));
    }
    return themes;
  }

  // The highest <version>_<n> folder of an extension.
  private static string? NewestVersionFolder(string extension) {
    try {
      return Directory.GetDirectories(extension)
          .Where(folder => File.Exists(Path.Combine(folder, ManifestFile)))
          .OrderBy(VersionKey, VersionComparer.Instance)
          .LastOrDefault();
    } catch (IOException) {
      return null;
    }
  }

  // "1.10.0_0" sorts after "1.9.0_0": the dotted numbers, not the text.
  internal static long[] VersionKey(string folder) =>
      Path.GetFileName(folder).Split('_')[0].Split('.')
          .Select(part => long.TryParse(part, out long number) ? number : 0)
          .ToArray();

  private static string? ThemeNameIn(string versionFolder) {
    JsonNode? manifest = EdgeUserData.ReadJson(Path.Combine(versionFolder, ManifestFile));
    return ThemeName(manifest, ReadMessages(versionFolder, manifest));
  }

  // The _locales/<locale>/messages.json for the manifest's default locale, else
  // English, else whichever locale exists.
  private static JsonNode? ReadMessages(string versionFolder, JsonNode? manifest) {
    string locales = Path.Combine(versionFolder, LocalesFolder);
    string? defaultLocale = AsString(manifest?["default_locale"]);
    foreach (string? preferred in new[] { defaultLocale, FallbackLocale }) {
      if (preferred is null) {
        continue;
      }
      JsonNode? messages = EdgeUserData.ReadJson(Path.Combine(locales, preferred, MessagesFile));
      if (messages is not null) {
        return messages;
      }
    }
    if (!Directory.Exists(locales)) {
      return null;
    }
    string? first = Directory.GetDirectories(locales).FirstOrDefault();
    return first is null ? null : EdgeUserData.ReadJson(Path.Combine(first, MessagesFile));
  }

  private sealed class VersionComparer : IComparer<long[]> {
    public static VersionComparer Instance { get; } = new();

    public int Compare(long[]? left, long[]? right) {
      left ??= [];
      right ??= [];
      for (int i = 0; i < Math.Min(left.Length, right.Length); i++) {
        int byPart = left[i].CompareTo(right[i]);
        if (byPart != 0) {
          return byPart;
        }
      }
      return left.Length.CompareTo(right.Length);
    }
  }
}
