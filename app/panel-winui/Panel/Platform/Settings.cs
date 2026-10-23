using System;
using System.Collections.Generic;
using System.IO;
using System.Text.Json;
using System.Text.Json.Serialization;

namespace EdgePlus;

internal enum Feature {
  NativeSplit,
  Theming,
  QuietDevMode,
  // protocol_handlers.h
  ProtocolHandlers,
  // find.h
  EnhancedFind,
  // theme_follow.h
  WebUiFollowsTheme,
  // live_preview.h
  LivePreview,
  // tab_shape.h
  TabShape,
  // reading_mode.h; the two themes are picked in reading mode itself and kept by the DLL.
  ReadingTheme,
}

[JsonConverter(typeof(JsonStringEnumConverter<ThemeSource>))]
internal enum ThemeSource {
  [JsonStringEnumMemberName("store")]
  Store,
  [JsonStringEnumMemberName("local")]
  Local,
}

internal enum ColorMode {
  Light,
  Dark,
}

// A settings file written when a removed switch existed still loads: an unknown
// field is ignored.
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
  // Saved under its old name so a saved choice carries over.
  [JsonPropertyName("tabSpine")]
  public bool TabShape { get; set; } = true;
  public bool ReadingTheme {
    get; set;
  }

  public bool IsOn(Feature feature) => feature switch {
    Feature.NativeSplit => NativeSplit,
    Feature.Theming => Theming,
    Feature.QuietDevMode => QuietDevMode,
    Feature.ProtocolHandlers => ProtocolHandlers,
    Feature.EnhancedFind => EnhancedFind,
    Feature.WebUiFollowsTheme => WebUiFollowsTheme,
    Feature.LivePreview => LivePreview,
    Feature.TabShape => TabShape,
    Feature.ReadingTheme => ReadingTheme,
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
      case Feature.TabShape:
        TabShape = isOn;
        break;
      case Feature.ReadingTheme:
        ReadingTheme = isOn;
        break;
      default:
        throw new ArgumentOutOfRangeException(nameof(feature));
    }
  }

  // QuietDevMode is the panel's own, not the DLL's.
  [JsonIgnore]
  public bool NeedsDll =>
      NativeSplit || Theming || ProtocolHandlers || EnhancedFind || WebUiFollowsTheme
      || LivePreview || TabShape || ReadingTheme;
}

internal sealed class ThemeEntry {
  public string Id { get; init; } = "";
  public string Name { get; init; } = "";
  public ThemeSource Source {
    get; init;
  }
  // The store URL or local path it was added from.
  public string Origin { get; init; } = "";
}

// An empty Light / Dark id means Edge's default theme.
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

  public void Reorder(IReadOnlyList<string> ids) => Themes = ThemeOrder.Ordered(Themes, ids);

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

// settings.json keeps exactly the shape the earlier Tauri panel wrote, so an
// existing file carries over; keep it so.
internal sealed class Settings {

  // Held by every load-change-save-apply sequence, so the UI's actions and the
  // background watchers never interleave on the file or on the DLL.
  public static object Gate { get; } = new();

  public Features Features { get; set; } = new();
  public Theming Theming { get; set; } = new();
  // A different build than this one running is recognized as an Edge update.
  public string? LastEdgeBuild {
    get; set;
  }

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

// Source-generated, not reflection-based: System.Text.Json's reflection path
// keeps a process-wide accessor cache that would pin this assembly and stop the
// host unloading it on a live swap.
[JsonSourceGenerationOptions(PropertyNamingPolicy = JsonKnownNamingPolicy.CamelCase,
                             WriteIndented = true)]
[JsonSerializable(typeof(Settings))]
internal sealed partial class SettingsJson : JsonSerializerContext;
