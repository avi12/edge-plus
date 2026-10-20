using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace EdgePlus;

// One feature the panel owns.
internal enum Feature {
  NativeSplit,
  Theming,
  QuietDevMode,
  // A site's request to handle a link type is only blocked when the user says
  // so (the DLL's protocol_handlers.h).
  ProtocolHandlers,
  // Edge's own Ctrl+F bar also takes /pattern/flags and "? question" (find.h).
  EnhancedFind,
  // edge://settings and edge://extensions take the theme's light or dark
  // (theme_follow.h).
  WebUiFollowsTheme,
  // A hovered tab shows a live preview instead of a frozen snapshot
  // (live_preview.h).
  LivePreview,
  // The selected vertical tab gets a square leading edge and the neon spine,
  // while theming is on (spine.h).
  TabSpine,
}

// Where a theme came from, so the installer knows how to add it to Edge. Stored
// lower-case, as the Tauri panel's settings.rs writes it.
[JsonConverter(typeof(JsonStringEnumConverter<ThemeSource>))]
internal enum ThemeSource {
  [JsonStringEnumMemberName("store")]
  Store,
  [JsonStringEnumMemberName("local")]
  Local,
}

// The two Windows appearance modes a theme can be assigned to.
internal enum ColorMode {
  Light,
  Dark,
}

// The feature switches, with their defaults (the split and the tab spine on,
// the rest off). A settings file written when a removed switch existed still
// loads: an unknown field is ignored.
internal sealed class Features {
  public bool NativeSplit { get; set; } = true;
  public bool Theming {
    get; set;
  }
  public bool QuietDevMode {
    get; set;
  }
  public bool ProtocolHandlers {
    get; set;
  }
  public bool EnhancedFind {
    get; set;
  }
  public bool WebUiFollowsTheme {
    get; set;
  }
  public bool LivePreview {
    get; set;
  }
  // On by default: it was part of the theming until it got a switch of its own.
  public bool TabSpine { get; set; } = true;

  public bool IsOn(Feature feature) => feature switch {
    Feature.NativeSplit => NativeSplit,
    Feature.Theming => Theming,
    Feature.QuietDevMode => QuietDevMode,
    Feature.ProtocolHandlers => ProtocolHandlers,
    Feature.EnhancedFind => EnhancedFind,
    Feature.WebUiFollowsTheme => WebUiFollowsTheme,
    Feature.LivePreview => LivePreview,
    Feature.TabSpine => TabSpine,
    _ => throw new ArgumentOutOfRangeException(nameof(feature)),
  };

  public void Set(Feature feature, bool isOn) {
    switch (feature) {
      case Feature.NativeSplit:
        NativeSplit = isOn;
        break;
      case Feature.Theming:
        Theming = isOn;
        break;
      case Feature.QuietDevMode:
        QuietDevMode = isOn;
        break;
      case Feature.ProtocolHandlers:
        ProtocolHandlers = isOn;
        break;
      case Feature.EnhancedFind:
        EnhancedFind = isOn;
        break;
      case Feature.WebUiFollowsTheme:
        WebUiFollowsTheme = isOn;
        break;
      case Feature.LivePreview:
        LivePreview = isOn;
        break;
      case Feature.TabSpine:
        TabSpine = isOn;
        break;
      default:
        throw new ArgumentOutOfRangeException(nameof(feature));
    }
  }

  // Whether any switch that lives in the injected DLL is on, so the DLL must be
  // loaded. Hiding the developer-mode prompt is the panel's own.
  [JsonIgnore]
  public bool NeedsDll =>
      NativeSplit || Theming || ProtocolHandlers || EnhancedFind || WebUiFollowsTheme
      || LivePreview || TabSpine;
}

// One theme the user added: an installed Edge theme extension by id.
internal sealed class ThemeEntry {
  public string Id { get; init; } = "";
  public string Name { get; init; } = "";
  public ThemeSource Source {
    get; init;
  }
  // The store URL or local path it was added from.
  public string Origin { get; init; } = "";
}

// The theme library and which theme to use in each Windows mode (an empty id
// means the default theme).
internal sealed class Theming {
  public List<ThemeEntry> Themes { get; set; } = [];
  public string Light { get; set; } = "";
  public string Dark { get; set; } = "";

  public string ThemeFor(ColorMode mode) => mode == ColorMode.Light ? Light : Dark;

  public void Assign(ColorMode mode, string id) {
    if (mode == ColorMode.Light) {
      Light = id;
    } else {
      Dark = id;
    }
  }

  public void AddOrReplace(ThemeEntry entry) {
    int index = Themes.FindIndex(theme => theme.Id == entry.Id);
    if (index >= 0) {
      Themes[index] = entry;
    } else {
      Themes.Add(entry);
    }
  }

  // Drop a theme, and any assignment to it falls back to the default theme.
  public void Remove(string id) {
    Themes.RemoveAll(theme => theme.Id == id);
    if (Light == id) {
      Light = "";
    }
    if (Dark == id) {
      Dark = "";
    }
  }
}

// The user's persisted choices: which switches are on, and the theme library
// with the light/dark assignment. Stored as settings.json in the data folder,
// in exactly the shape the Tauri panel writes (settings.rs), so the two panels
// read each other's file. Only this file is the panel's own; every other file
// is a contract with Edge or the DLL.
internal sealed class Settings {

  // Every load-change-save-apply sequence holds this, so the UI's actions and
  // the background watchers (EdgeWatch, ThemeWatch) never interleave on the
  // file or on the DLL.
  public static object Gate { get; } = new();

  public Features Features { get; set; } = new();
  public Theming Theming { get; set; } = new();
  // The msedge.dll build last seen running, so a different one is recognized
  // as an Edge update.
  public string? LastEdgeBuild {
    get; set;
  }

  // Read the settings file, falling back to defaults when it is missing or
  // unreadable.
  public static Settings Load() => Load(Paths.Settings);

  public static Settings Load(string path) {
    try {
      return Parse(File.ReadAllText(path));
    } catch (IOException) {
      return new Settings();
    } catch (UnauthorizedAccessException) {
      return new Settings();
    }
  }

  public static Settings Parse(string text) {
    Settings? parsed;
    try {
      parsed = JsonSerializer.Deserialize(text, SettingsJson.Default.Settings);
    } catch (JsonException) {
      return new Settings();
    }
    if (parsed is null) {
      return new Settings();
    }
    // An explicit null in the file is a missing section, not a crash later.
    parsed.Features ??= new Features();
    parsed.Theming ??= new Theming();
    parsed.Theming.Themes ??= [];
    parsed.Theming.Light ??= "";
    parsed.Theming.Dark ??= "";
    return parsed;
  }

  public string ToJson() => JsonSerializer.Serialize(this, SettingsJson.Default.Settings);

  public void Save() => Save(Paths.Settings);

  public void Save(string path) => Paths.WriteAtomically(path, ToJson());
}

// The settings file's serializer, generated at build time rather than built by
// reflection at run time: System.Text.Json's reflection path keeps a
// process-wide cache of the members it emitted accessors for, which would hold
// this assembly and stop the host unloading it on a live swap.
[JsonSourceGenerationOptions(PropertyNamingPolicy = JsonKnownNamingPolicy.CamelCase,
                             WriteIndented = true)]
[JsonSerializable(typeof(Settings))]
internal sealed partial class SettingsJson : JsonSerializerContext;
