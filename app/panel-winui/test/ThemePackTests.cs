using System;
using System.IO;
using System.IO.Compression;
using System.Security.Cryptography;

namespace EdgePlus;

internal static class ThemePackTests {
  private const string Id = "aaaabbbbccccddddeeeeffffgggghhhh";
  private static readonly byte[] s_png = [0x89, (byte)'P', (byte)'N', (byte)'G', 1, 2, 3];

  public static void Run() {
    ExtensionIdsAreThirtyTwoLettersAToP();
    StoreLinksYieldIdNameAndStore();
    NotACrxIsRefused();
    APackedFolderReadsBackSignedAndStable();
    AZipPacksLikeAFolder();
    BrokenThemesAreRefusedWithAReason();
    TheKeyFileIsNamedByTheLowerCasedPath();
  }

  private static void ExtensionIdsAreThirtyTwoLettersAToP() {
    Expect.That(ExtensionId.IsValid(Id), "a..p, 32 long");
    Expect.That(!ExtensionId.IsValid(Id[..31]), "31 is too short");
    Expect.That(!ExtensionId.IsValid(Id[..31] + "q"), "q is not in the alphabet");
    string fromKey = ExtensionId.FromPublicKey([1, 2, 3]);
    Expect.That(ExtensionId.IsValid(fromKey), "an id from a key is an id");
    Expect.Equal(fromKey, ExtensionId.FromPublicKey([1, 2, 3]), "the same key, the same id");
  }

  private static void StoreLinksYieldIdNameAndStore() {
    ParsedStoreTheme? chrome = StoreUrl.Parse(
        $"  https://chromewebstore.google.com/detail/dusk-at_sea/{Id}?hl=en#reviews ");
    Expect.That(chrome is not null, "a Chrome Web Store link parses");
    Expect.Equal(Id, chrome!.Id, "the id is the 32-letter segment");
    Expect.Equal("Dusk At Sea", chrome.Name, "the slug becomes a title");
    Expect.Equal(ThemeStore.Chrome, chrome.Store, "google's host is the Chrome store");

    ParsedStoreTheme? edge = StoreUrl.Parse(
        $"https://microsoftedge.microsoft.com/addons/detail/night/{Id}");
    Expect.Equal(ThemeStore.Edge, edge?.Store, "microsoft's host is Edge Add-ons");
    Expect.Equal("Night", edge?.Name, "one word is capitalized");

    Expect.Equal("A" + Id[1..], StoreUrl.Parse($"https://example.com/{Id}")?.Name,
                 "with no slug the id is the name, titled like a slug");
    Expect.That(StoreUrl.Parse($"ftp://x/{Id}") is null, "only http(s) links");
    Expect.That(StoreUrl.Parse("https://chromewebstore.google.com/detail/x/short") is null,
                "a link with no id");
  }

  private static void NotACrxIsRefused() {
    Expect.That(Throws(() => Crx.Read([1, 2, 3])), "three bytes are not a CRX");
    byte[] wrongVersion = [.. "Cr24"u8, 2, 0, 0, 0, 0, 0, 0, 0, 0];
    Expect.That(Throws(() => Crx.Read(wrongVersion)), "CRX2 is refused");
    byte[] overlong = [.. "Cr24"u8, 3, 0, 0, 0, 0xff, 0, 0, 0, 0];
    Expect.That(Throws(() => Crx.Read(overlong)), "a header longer than the file is refused");
  }

  private static void APackedFolderReadsBackSignedAndStable() {
    string root = TemporaryFolder("pack");
    string theme = Path.Combine(root, "theme");
    Directory.CreateDirectory(Path.Combine(theme, "images"));
    File.WriteAllText(Path.Combine(theme, "manifest.json"),
                      """{"manifest_version":3,"name":"t","version":"1.2.3","theme":{"images":{"theme_frame":"images/frame.png"},"colors":{"frame":[1,2,3,0.5]},"tints":{"buttons":[-1,0.5,1]}}}""");
    File.WriteAllBytes(Path.Combine(theme, "images", "frame.png"), s_png);
    string keys = Path.Combine(root, "keys");

    PackedTheme first = ThemePacker.Pack(theme, keys);
    CrxInfo info = Crx.Read(first.Crx);
    Expect.Equal(first.Id, info.Id, "the id reads back from the CRX");
    Expect.Equal("1.2.3.1", info.Version, "the first pack is revision 1");
    Expect.Equal("1.2.3.1", first.Version, "the packer reports the stamped version");

    CrxProof proof = Crx.ReadProof(first.Crx);
    using RSA key = RSA.Create();
    key.ImportSubjectPublicKeyInfo(proof.PublicKey, out _);
    byte[] signedPayload = ThemePacker.SignedPayload(proof.SignedHeaderData, Crx.Zip(first.Crx));
    bool isSigned = key.VerifyData(signedPayload, proof.Signature, HashAlgorithmName.SHA256,
                                   RSASignaturePadding.Pkcs1);
    Expect.That(isSigned, "the signature verifies against the header's own key");
    byte[] crxId = SHA256.HashData(proof.PublicKey)[..16];
    Expect.That(proof.SignedHeaderData.AsSpan().EndsWith(crxId),
                "the signed header names the key's crx id");

    PackedTheme second = ThemePacker.Pack(theme, keys);
    Expect.Equal(first.Id, second.Id, "packing again keeps the id");
    Expect.Equal("1.2.3.2", second.Version, "packing again raises the revision");
    using var zip = new ZipArchive(new MemoryStream(Crx.Zip(second.Crx)));
    Expect.That(zip.GetEntry("images/frame.png") is not null, "folders are zipped with '/'");
    Directory.Delete(root, recursive: true);
  }

  private static void AZipPacksLikeAFolder() {
    string root = TemporaryFolder("zip");
    string zipPath = Path.Combine(root, "theme.zip");
    using (ZipArchive archive = ZipFile.Open(zipPath, ZipArchiveMode.Create)) {
      WriteEntry(archive, "manifest.json", """{"name":"z","version":"2","theme":{}}"""u8.ToArray());
    }
    PackedTheme packed = ThemePacker.Pack(zipPath, Path.Combine(root, "keys"));
    Expect.Equal("2.0.0.1", packed.Version, "a short version is padded to three parts");
    Directory.Delete(root, recursive: true);
  }

  private static void BrokenThemesAreRefusedWithAReason() {
    ExpectRefused("""{"name":"t","version":"1","theme":{"images":{"x":"gone.png"}}}""",
                  "image x (gone.png): gone.png is missing", "a missing image");
    ExpectRefused("""{"name":"t","version":"1","theme":{"images":{"x":"manifest.json"}}}""",
                  "image x (manifest.json) is not a PNG or JPEG", "a non-image");
    ExpectRefused("""{"name":"t","version":"1","theme":{"colors":{"frame":[300,0,0]}}}""",
                  "color frame has a value out of range", "a channel past 255");
    ExpectRefused("""{"name":"t","version":"1","theme":{"colors":{"frame":[1.5,0,0]}}}""",
                  "color frame has a value out of range", "a fractional channel");
    ExpectRefused("""{"name":"t","version":"1","theme":{"colors":{"frame":[0,0]}}}""",
                  "color frame needs 3 or 4 numbers", "two channels");
    ExpectRefused("""{"name":"t","version":"1","theme":{"tints":{"b":[2,0,0]}}}""",
                  "tint b has a value out of range", "a tint past 1");
    ExpectRefused("""{"name":"t","version":"1"}""",
                  "manifest.json has no \"theme\" section", "an extension that is not a theme");
    ExpectRefused("""{"name":"t","version":"1.x","theme":{}}""",
                  "\"1.x\" is not a valid version", "a bad version");
    ExpectRefused("""{"version":"1","theme":{}}""", "manifest.json has no name", "no name");
    ExpectRefused("{\n  \"name\": oops }", "manifest.json is not valid JSON (line 2)",
                  "broken JSON is said plainly, with its line, not as the parser's message");
  }

  private static void TheKeyFileIsNamedByTheLowerCasedPath() {
    string upper = ThemePacker.KeyFileName(@"C:\Themes\Dusk");
    Expect.Equal(upper, ThemePacker.KeyFileName(@"c:\themes\dusk"), "case does not matter");
    Expect.That(upper.Length == 16 + ".key".Length && upper.EndsWith(".key"),
                "sixteen hex digits and .key");
  }

  private static void ExpectRefused(string manifest, string reason, string what) {
    using var output = new MemoryStream();
    using (var archive = new ZipArchive(output, ZipArchiveMode.Create, leaveOpen: true)) {
      WriteEntry(archive, "manifest.json", System.Text.Encoding.UTF8.GetBytes(manifest));
    }
    string? message = null;
    try {
      ThemeManifest.Validate(output.ToArray());
    } catch (InvalidDataException error) {
      message = error.Message;
    }
    Expect.Equal(reason, message, what);
  }

  private static void WriteEntry(ZipArchive archive, string name, byte[] bytes) {
    using Stream stream = archive.CreateEntry(name).Open();
    stream.Write(bytes);
  }

  private static string TemporaryFolder(string purpose) {
    string folder = Path.Combine(Path.GetTempPath(),
                                 $"edgeplus-{purpose}-{Environment.ProcessId}");
    if (Directory.Exists(folder)) {
      Directory.Delete(folder, recursive: true);
    }
    Directory.CreateDirectory(folder);
    return folder;
  }

  private static bool Throws(Action action) {
    try {
      action();
      return false;
    } catch (InvalidDataException) {
      return true;
    }
  }
}
