using System;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace EdgePlus;

// A theme the user edits live is re-installed into the running Edge, so a
// broken edit must be caught here rather than hot-swapped in. Its messages are
// the user's.
internal static class ThemeManifest {
  public const string ManifestName = "manifest.json";
  private const long MaxThemeBytes = 64L * 1024 * 1024;
  private const long MaxImageBytes = 16L * 1024 * 1024;
  private const int MaxVersionParts = 4;
  private const int RevisionPart = 3;
  private const long ColorChannelMax = 255;
  private const int RgbChannels = 3;
  private const int RgbaChannels = 4;
  private const int AlphaIndex = 3;
  private const int TintParts = 3;
  private const double TintUnchanged = -1.0;
  private static readonly byte[] s_pngMagic = [0x89, (byte)'P', (byte)'N', (byte)'G'];
  private static readonly byte[] s_jpegMagic = [0xff, 0xd8, 0xff];

  // Rejects what Edge's theme loader would trip over.
  public static void Validate(byte[] zipBytes) {
    using ZipArchive archive = Open(zipBytes);
    long total = archive.Entries.Sum(entry => entry.Length);
    if (total > MaxThemeBytes) {
      throw Invalid("the theme is too large");
    }
    JsonObject manifest = ReadManifest(archive);
    bool hasName = manifest["name"] is JsonValue name && name.GetValueKind() == JsonValueKind.String;
    if (!hasName) {
      throw Invalid("manifest.json has no name");
    }
    ParseVersion(manifest);
    if (manifest["theme"] is not JsonObject theme) {
      throw Invalid("manifest.json has no \"theme\" section");
    }
    if (theme["images"] is JsonObject images) {
      foreach ((string key, JsonNode? value) in images) {
        bool isPath = value is JsonValue path && path.GetValueKind() == JsonValueKind.String;
        if (!isPath) {
          throw Invalid($"image {key} is not a path");
        }
        CheckImage(archive, key, value!.GetValue<string>());
      }
    }
    if (theme["colors"] is JsonObject colors) {
      foreach ((string key, JsonNode? value) in colors) {
        CheckColor(key, value);
      }
    }
    if (theme["tints"] is JsonObject tints) {
      foreach ((string key, JsonNode? value) in tints) {
        CheckTint(key, value);
      }
    }
  }

  // Every re-pack needs a higher version: Edge only replaces an installed
  // extension with a newer one. The original's first three parts are kept.
  public static (byte[] Zip, string Version) Revise(byte[] zipBytes, ushort revision) {
    using ZipArchive source = Open(zipBytes);
    JsonObject manifest = ReadManifest(source);
    ushort[] parts = ParseVersion(manifest);
    ushort[] revised = [.. parts.Concat(Enumerable.Repeat((ushort)0, RevisionPart)).Take(RevisionPart),
                        revision];
    string version = string.Join('.', revised);
    manifest["version"] = version;

    using var output = new MemoryStream();
    using (var writer = new ZipArchive(output, ZipArchiveMode.Create, leaveOpen: true)) {
      foreach (ZipArchiveEntry entry in source.Entries) {
        bool isManifest = entry.FullName == ManifestName;
        if (isManifest) {
          continue;
        }
        ZipArchiveEntry copy = writer.CreateEntry(entry.FullName);
        using Stream from = entry.Open();
        using Stream to = copy.Open();
        from.CopyTo(to);
      }
      ZipArchiveEntry revisedManifest = writer.CreateEntry(ManifestName);
      using Stream manifestStream = revisedManifest.Open();
      manifestStream.Write(Encoding.UTF8.GetBytes(manifest.ToJsonString()));
    }
    return (output.ToArray(), version);
  }

  private static InvalidDataException Invalid(string message) => new(message);

  private static ZipArchive Open(byte[] zipBytes) {
    try {
      return new ZipArchive(new MemoryStream(zipBytes), ZipArchiveMode.Read);
    } catch (InvalidDataException) {
      throw Invalid("not a valid ZIP");
    }
  }

  private static byte[] ReadEntry(ZipArchive archive, string name, long limit) {
    ZipArchiveEntry entry = archive.GetEntry(name) ?? throw Invalid($"{name} is missing");
    if (entry.Length > limit) {
      throw Invalid($"{name} is too large");
    }
    using Stream stream = entry.Open();
    using var bytes = new MemoryStream();
    stream.CopyTo(bytes);
    return bytes.ToArray();
  }

  private static JsonObject ReadManifest(ZipArchive archive) {
    byte[] text = ReadEntry(archive, ManifestName, MaxImageBytes);
    try {
      return JsonNode.Parse(text) as JsonObject ?? throw Invalid("manifest.json: not an object");
    } catch (JsonException error) {
      // The parser's own message is for a developer; its position is kept.
      throw Invalid($"{Wording.ManifestNotJson} (line {error.LineNumber + 1})");
    }
  }

  private static void CheckImage(ZipArchive archive, string key, string path) {
    byte[] bytes;
    try {
      bytes = ReadEntry(archive, path, MaxImageBytes);
    } catch (InvalidDataException error) {
      throw Invalid($"image {key} ({path}): {error.Message}");
    }
    bool isImage = bytes.AsSpan().StartsWith(s_pngMagic) || bytes.AsSpan().StartsWith(s_jpegMagic);
    if (!isImage) {
      throw Invalid($"image {key} ({path}) is not a PNG or JPEG");
    }
  }

  private static void CheckColor(string key, JsonNode? value) {
    if (value is not JsonArray channels) {
      throw Invalid($"color {key} is not a list");
    }
    bool isRgb = channels.Count == RgbChannels;
    bool isRgba = channels.Count == RgbaChannels;
    if (!isRgb && !isRgba) {
      throw Invalid($"color {key} needs 3 or 4 numbers");
    }
    for (int index = 0; index < channels.Count; index++) {
      bool isAlpha = index == AlphaIndex;
      bool isValid = isAlpha
          ? AsDouble(channels[index]) is double alpha && alpha is >= 0.0 and <= 1.0
          : AsInteger(channels[index]) is long channel && channel is >= 0 and <= ColorChannelMax;
      if (!isValid) {
        throw Invalid($"color {key} has a value out of range");
      }
    }
  }

  private static void CheckTint(string key, JsonNode? value) {
    bool isThreeParts = value is JsonArray parts && parts.Count == TintParts;
    if (!isThreeParts) {
      throw Invalid($"tint {key} needs 3 numbers");
    }
    bool isValid = ((JsonArray)value!).All(part => AsDouble(part) is double number
        && (number == TintUnchanged || number is >= 0.0 and <= 1.0));
    if (!isValid) {
      throw Invalid($"tint {key} has a value out of range");
    }
  }

  // Null for a fraction: 255.0 is not a channel.
  private static long? AsInteger(JsonNode? node) {
    bool isNumber = node is JsonValue value && value.GetValueKind() == JsonValueKind.Number;
    if (!isNumber) {
      return null;
    }
    return ((JsonValue)node!).TryGetValue(out long whole) ? whole : null;
  }

  private static double? AsDouble(JsonNode? node) {
    bool isNumber = node is JsonValue value && value.GetValueKind() == JsonValueKind.Number;
    return isNumber ? ((JsonValue)node!).GetValue<double>() : null;
  }

  private static ushort[] ParseVersion(JsonObject manifest) {
    bool hasVersion = manifest["version"] is JsonValue value
        && value.GetValueKind() == JsonValueKind.String;
    if (!hasVersion) {
      throw Invalid("manifest.json has no version");
    }
    string text = manifest["version"]!.GetValue<string>();
    string[] pieces = text.Split('.');
    if (pieces.Length > MaxVersionParts) {
      throw Invalid($"\"{text}\" is not a valid version");
    }
    var parts = new ushort[pieces.Length];
    for (int i = 0; i < pieces.Length; i++) {
      bool isPart = ushort.TryParse(pieces[i], NumberStyles.None, CultureInfo.InvariantCulture,
                                    out parts[i]);
      if (!isPart) {
        throw Invalid($"\"{text}\" is not a valid version");
      }
    }
    return parts;
  }
}
