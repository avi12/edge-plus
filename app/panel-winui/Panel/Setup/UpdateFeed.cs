using System;
using System.Linq;
using System.Text.Json;

namespace EdgePlus;

// A release newer than the running one: the exe to fetch and the digest to
// check it by.
internal sealed record UpdateOffer(Version Version, Uri Download, string Sha256);

// Reads the update manifest the repository hosts (docs/updates.json, written by
// tools/Write-UpdateManifest.ps1 once a release's exe is up): the latest
// version, where its exe is, and the exe's SHA-256. It is the same arrangement
// as a browser extension's update URL: the app polls a small file and nothing
// is offered unless the file names a newer version. Without a digest nothing is
// offered, since a download that cannot be checked is never swapped in for the
// running exe.
internal static class UpdateFeed {
  public const string DefaultUrl =
      "https://raw.githubusercontent.com/avi12/edge-plus/main/docs/updates.json";
  // Set by a test to point the check at a local stand-in for GitHub.
  public const string UrlVariable = "EDGEPLUS_UPDATE_FEED";

  private const int Sha256HexLength = 64;

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
    string digest = Text(manifest, "sha256");
    bool hasDigest = digest.Length == Sha256HexLength && digest.All(Uri.IsHexDigit);
    bool isUrl = Uri.TryCreate(Text(manifest, "url"), UriKind.Absolute, out Uri? url);
    bool isOffered = isVersion && version > current && hasDigest && isUrl && IsTrusted(url!);
    return isOffered ? new UpdateOffer(version!, url!, digest.ToUpperInvariant()) : null;
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
