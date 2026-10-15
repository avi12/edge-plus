using System;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using Microsoft.Win32;

namespace FixEdge;

// Adds a theme to Edge without touching Edge's ABI: it writes an entry under
// Edge's own external-extensions registry key, and Edge's loader installs it.
// A store theme points at the store's update URL, so Edge installs it as a
// trusted theme (no developer mode); a local theme points at a .crx on disk.
// The install lands when Edge next loads the extension — the panel marks the
// theme "installing" until the DLL confirms it is applied.
internal static class ThemeInstaller {
  // Edge reads externally-added extensions from here (HKCU, per-user).
  private const string ExtensionsKeyPath = @"Software\Microsoft\Edge\Extensions";
  private const string UpdateUrlValue = "update_url";
  private const string PathValue = "path";
  private const string VersionValue = "version";

  private const string ChromeStoreUpdateUrl =
      "https://clients2.google.com/service/update2/crx";
  private const string EdgeStoreUpdateUrl =
      "https://edge.microsoft.com/extensionwebstorebase/v1/crx";

  private static readonly char[] s_idAlphabet = "abcdefghijklmnop".ToCharArray();

  // Which store a theme came from, so the matching update URL is used.
  internal enum Store {
    Chrome,
    Edge,
  }

  internal readonly record struct ParsedStoreTheme(string Id, string Name, Store Store);

  // Pull the extension id, a display name and the store from a pasted store URL.
  // Edge Add-ons and Chrome Web Store both end /detail/<slug>/<id>.
  public static bool TryParseStoreUrl(string url, out ParsedStoreTheme parsed) {
    parsed = default;
    if (!Uri.TryCreate(url.Trim(), UriKind.Absolute, out Uri? uri)) {
      return false;
    }
    string[] segments =
        uri.AbsolutePath.Split('/', StringSplitOptions.RemoveEmptyEntries);
    string? id = segments.LastOrDefault(IsExtensionId);
    if (id is null) {
      return false;
    }
    int idIndex = Array.LastIndexOf(segments, id);
    string slug = idIndex > 0 ? segments[idIndex - 1] : id;
    bool isEdgeStore =
        uri.Host.Contains("microsoft", StringComparison.OrdinalIgnoreCase);
    parsed = new ParsedStoreTheme(id, TitleFromSlug(slug),
                                  isEdgeStore ? Store.Edge : Store.Chrome);
    return true;
  }

  public static void InstallStore(string extensionId, Store store) {
    using RegistryKey key = OpenExtensionKey(extensionId);
    key.SetValue(UpdateUrlValue,
                 store == Store.Edge ? EdgeStoreUpdateUrl : ChromeStoreUpdateUrl);
  }

  // Read a local theme .crx: its embedded extension id and the version Edge
  // needs, copy it into the panel's data folder, and register it by path.
  public static ThemeEntry InstallLocalCrx(string crxPath) {
    byte[] bytes = File.ReadAllBytes(crxPath);
    string id = ReadCrxId(bytes);
    string version = ReadCrxVersion(bytes);
    string name = Path.GetFileNameWithoutExtension(crxPath);
    string stored = Path.Combine(Paths.ThemesRoot, id + ".crx");
    Directory.CreateDirectory(Paths.ThemesRoot);
    File.Copy(crxPath, stored, overwrite: true);
    using RegistryKey key = OpenExtensionKey(id);
    key.SetValue(PathValue, stored);
    key.SetValue(VersionValue, version);
    return new ThemeEntry(id, name, ThemeSource.Local, crxPath);
  }

  // Drop a theme's external-extensions entry (best effort). Edge removes the
  // extension on its next start.
  public static void Uninstall(string extensionId) {
    try {
      using RegistryKey? parent =
          Registry.CurrentUser.OpenSubKey(ExtensionsKeyPath, writable: true);
      parent?.DeleteSubKeyTree(extensionId, throwOnMissingSubKey: false);
    } catch (Exception exception) when (exception is UnauthorizedAccessException
                                        or IOException) {
      // Leaving the key is harmless; the theme simply stays installable.
    }
  }

  private static RegistryKey OpenExtensionKey(string extensionId) =>
      Registry.CurrentUser.CreateSubKey($@"{ExtensionsKeyPath}\{extensionId}");

  // An extension id is 32 characters of a..p.
  private static bool IsExtensionId(string value) =>
      value.Length == 32 && value.All(character => character is >= 'a' and <= 'p');

  private static string TitleFromSlug(string slug) {
    string spaced = slug.Replace('-', ' ').Replace('_', ' ').Trim();
    return spaced.Length == 0
        ? slug
        : CultureInfo.InvariantCulture.TextInfo.ToTitleCase(spaced);
  }

  // The id of a CRX3 file is the first 16 bytes of the SHA-256 of its public
  // key, each nibble mapped to a..p. The public key is in the Cr24 header.
  private static string ReadCrxId(byte[] crx) {
    byte[] publicKey = ReadCrxPublicKey(crx);
    byte[] hash = SHA256.HashData(publicKey);
    var id = new StringBuilder(32);
    for (int i = 0; i < 16; i++) {
      id.Append(s_idAlphabet[hash[i] >> 4]);
      id.Append(s_idAlphabet[hash[i] & 0xf]);
    }
    return id.ToString();
  }

  // Walk the CRX3 header: "Cr24", version 3, header length, then a CrxFileHeader
  // protobuf whose repeated AsymmetricKeyProof (field 2) carries the RSA public
  // key (its field 1).
  private static byte[] ReadCrxPublicKey(byte[] crx) {
    bool isCrx3 = crx.Length > 12 && crx[0] == 'C' && crx[1] == 'r' && crx[2] == '2'
                  && crx[3] == '4' && BitConverter.ToUInt32(crx, 4) == 3;
    if (!isCrx3) {
      throw new InvalidDataException("not a CRX3 file");
    }
    int headerLength = (int)BitConverter.ToUInt32(crx, 8);
    int offset = 12;
    int headerEnd = offset + headerLength;
    while (offset < headerEnd) {
      (int fieldNumber, int wireType, int next) = ReadTag(crx, offset);
      offset = next;
      if (wireType != 2) {
        throw new InvalidDataException("unexpected CRX header field");
      }
      (int length, int dataStart) = ReadVarint(crx, offset);
      offset = dataStart;
      // Field 2 = sha256_with_rsa (AsymmetricKeyProof); its field 1 is the key.
      if (fieldNumber == 2) {
        return ReadProofPublicKey(crx, offset, offset + length);
      }
      offset += length;
    }
    throw new InvalidDataException("no public key in CRX header");
  }

  private static byte[] ReadProofPublicKey(byte[] crx, int start, int end) {
    int offset = start;
    while (offset < end) {
      (int fieldNumber, int wireType, int next) = ReadTag(crx, offset);
      offset = next;
      (int length, int dataStart) = ReadVarint(crx, offset);
      offset = dataStart;
      if (fieldNumber == 1 && wireType == 2) {
        return crx[offset..(offset + length)];
      }
      offset += length;
    }
    throw new InvalidDataException("no public key in key proof");
  }

  private static string ReadCrxVersion(byte[] crx) {
    int headerLength = (int)BitConverter.ToUInt32(crx, 8);
    int zipStart = 12 + headerLength;
    using var zip = new ZipArchive(new MemoryStream(crx, zipStart, crx.Length - zipStart));
    ZipArchiveEntry? manifest = zip.GetEntry("manifest.json");
    if (manifest is null) {
      throw new InvalidDataException("no manifest.json in .crx");
    }
    using Stream stream = manifest.Open();
    using JsonDocument document = JsonDocument.Parse(stream);
    return document.RootElement.GetProperty("version").GetString() ?? "0";
  }

  private static (int fieldNumber, int wireType, int next) ReadTag(byte[] data, int offset) {
    (int tag, int next) = ReadVarint(data, offset);
    return (tag >> 3, tag & 0x7, next);
  }

  private static (int value, int next) ReadVarint(byte[] data, int offset) {
    int value = 0;
    int shift = 0;
    while (true) {
      byte piece = data[offset++];
      value |= (piece & 0x7f) << shift;
      if ((piece & 0x80) == 0) {
        return (value, offset);
      }
      shift += 7;
    }
  }
}
