using System;
using System.Linq;
using System.Text.Json;

namespace EdgePlus;

// One file of a release: where to fetch it and the digest to check it by.
internal sealed record UpdateAsset(Uri Download, string Sha256);

// A release newer than the running one: its portable exe, and its MSI for a
// copy the MSI installed (null when the manifest names none).
internal sealed record UpdateOffer(Version Version, UpdateAsset Exe, UpdateAsset? Installer);

// Reads the update manifest the repository hosts (docs/updates.json, written by
// tools/Write-UpdateManifest.ps1 once a release's files are up): the latest
// version, where its exe and its MSI are, and each one's SHA-256. It is the
// same arrangement as a browser extension's update URL: the app polls a small
// file and nothing is offered unless the file names a newer version. A file
// without a digest is never offered, since a download that cannot be checked is
// never installed in place of the running exe.
internal static class UpdateFeed {
  public const string DefaultUrl =
      "https://raw.githubusercontent.com/avi12/edge-plus/main/docs/updates.json";
  // Set by a test to point the check at a local stand-in for GitHub.
  public const string UrlVariable = "EDGEPLUS_UPDATE_FEED";

  private const int Sha256HexLength = 64;
  private const string InstallerProperty = "installer";

  public static string Url =>
      Environment.GetEnvironmentVariable(UrlVariable) is { Length: > 0 } custom ? custom : DefaultUrl;

  public static UpdateOffer? Parse(string json, Version current) {
    try {
      using JsonDocument document = JsonDocument.Parse(json);
      return Offer(document.RootElement, current);
    } catch (JsonException) {
      return null;
    }
  }

  private static UpdateOffer? Offer(JsonElement manifest, Version current) {
    bool isVersion = Version.TryParse(Text(manifest, "version"), out Version? version);
    UpdateAsset? exe = Asset(manifest);
    bool isOffered = isVersion && version > current && exe is not null;
    if (!isOffered) {
      return null;
    }
    // A manifest with a version is an object, so it can be asked for a property.
    UpdateAsset? installer = manifest.TryGetProperty(InstallerProperty, out JsonElement entry)
        ? Asset(entry)
        : null;
    return new UpdateOffer(version!, exe!, installer);
  }

  // The `url` and `sha256` of a file, or null unless both are sound.
  private static UpdateAsset? Asset(JsonElement element) {
    string digest = Text(element, "sha256");
    bool hasDigest = digest.Length == Sha256HexLength && digest.All(Uri.IsHexDigit);
    bool isUrl = Uri.TryCreate(Text(element, "url"), UriKind.Absolute, out Uri? url);
    bool isSound = hasDigest && isUrl && IsTrusted(url!);
    return isSound ? new UpdateAsset(url!, digest.ToUpperInvariant()) : null;
  }

  // Over https, or from this machine for a test's stand-in server.
  private static bool IsTrusted(Uri url) =>
      url.Scheme == Uri.UriSchemeHttps || (url.Scheme == Uri.UriSchemeHttp && url.IsLoopback);

  private static string Text(JsonElement element, string name) =>
      element.ValueKind == JsonValueKind.Object
      && element.TryGetProperty(name, out JsonElement value)
      && value.ValueKind == JsonValueKind.String
          ? value.GetString() ?? ""
          : "";
}
