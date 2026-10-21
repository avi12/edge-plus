using System;
using System.Buffers.Binary;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Security.Cryptography;
using System.Text;

namespace EdgePlus;

// A theme ready to hand to Edge's external-extension loader.
internal sealed record PackedTheme(string Id, string Version, byte[] Crx);

// Turning a local theme folder or .zip into a signed .crx.
//
// Edge's external-extension loader installs only a .crx, so a theme given as an
// unpacked folder or a ZIP is packed here: zipped, signed with an RSA key and
// wrapped in a CRX3 header. The key is kept per source path, so adding the same
// theme again keeps its extension id.
//
// The key file holds Windows CNG's RSAPRIVATEBLOB, the format the earlier Tauri
// panel wrote into the same folder, so a theme it packed keeps its id when this
// panel packs it again; keep the format and the file names.
internal static class ThemePacker {
  private const int KeyBits = 2048;
  private const int CrxIdBytes = 16;
  private const int KeyFileNameBytes = 8;
  private const string KeyExtension = ".key";
  private const string RevisionExtension = ".rev";
  private const string KeyLengthProperty = "Length";
  private static readonly CngKeyBlobFormat s_privateBlob = new("RSAPRIVATEBLOB");

  // Pack the theme at `source` (an unpacked folder, or a .zip with its
  // manifest.json at the top) into a signed CRX3. `keyDirectory` holds the
  // per-theme signing keys. Throws InvalidDataException for a broken theme.
  public static PackedTheme Pack(string source, string keyDirectory) {
    byte[] zipBytes = Directory.Exists(source) ? ZipFolder(source) : File.ReadAllBytes(source);
    ThemeManifest.Validate(zipBytes);
    Directory.CreateDirectory(keyDirectory);
    string keyFile = Path.Combine(keyDirectory, KeyFileName(source));
    (byte[] zip, string version) = ThemeManifest.Revise(zipBytes, NextRevision(keyFile));
    using RSA signer = LoadOrCreateKey(keyFile);
    byte[] publicKey = signer.ExportSubjectPublicKeyInfo();
    string id = ExtensionId.FromPublicKey(publicKey);

    byte[] crxId = SHA256.HashData(publicKey)[..CrxIdBytes];
    byte[] signedHeaderData = Protobuf.LengthDelimitedField(Crx.CrxIdField, crxId);
    byte[] signature = signer.SignData(SignedPayload(signedHeaderData, zip),
                                       HashAlgorithmName.SHA256, RSASignaturePadding.Pkcs1);

    byte[] proof = [.. Protobuf.LengthDelimitedField(Crx.PublicKeyField, publicKey),
                    .. Protobuf.LengthDelimitedField(Crx.SignatureField, signature)];
    byte[] header = [.. Protobuf.LengthDelimitedField(Crx.RsaProofField, proof),
                     .. Protobuf.LengthDelimitedField(Crx.SignedHeaderDataField,
                                                      signedHeaderData)];
    byte[] crx = [.. Crx.Magic, .. LittleEndian(Crx.FormatVersion),
                  .. LittleEndian((uint)header.Length), .. header, .. zip];
    return new PackedTheme(id, version, crx);
  }

  // What a CRX3 signature covers: the fixed prefix, the signed header data's
  // length and bytes, then the ZIP.
  public static byte[] SignedPayload(byte[] signedHeaderData, byte[] zip) =>
      [.. Crx.SignedDataPrefix, .. LittleEndian((uint)signedHeaderData.Length),
       .. signedHeaderData, .. zip];

  // The key's file name: a hash of the lower-cased source path.
  public static string KeyFileName(string source) {
    byte[] hash = SHA256.HashData(Encoding.UTF8.GetBytes(source.ToLowerInvariant()));
    ulong prefix = BinaryPrimitives.ReadUInt64BigEndian(hash.AsSpan(0, KeyFileNameBytes));
    return prefix.ToString("x16", CultureInfo.InvariantCulture) + KeyExtension;
  }

  // The theme's pack count, kept beside its key: each pack gets a higher
  // version, which is what makes Edge replace the installed copy.
  private static ushort NextRevision(string keyFile) {
    string counterFile = Path.ChangeExtension(keyFile, RevisionExtension);
    ushort previous = 0;
    if (File.Exists(counterFile)) {
      _ = ushort.TryParse(File.ReadAllText(counterFile).Trim(), NumberStyles.None,
                          CultureInfo.InvariantCulture, out previous);
    }
    ushort next = previous == ushort.MaxValue ? (ushort)1 : (ushort)(previous + 1);
    File.WriteAllText(counterFile, next.ToString(CultureInfo.InvariantCulture));
    return next;
  }

  private static RSA LoadOrCreateKey(string keyFile) {
    if (File.Exists(keyFile)) {
      return new RSACng(CngKey.Import(File.ReadAllBytes(keyFile), s_privateBlob));
    }
    var parameters = new CngKeyCreationParameters {
      ExportPolicy = CngExportPolicies.AllowPlaintextExport,
    };
    parameters.Parameters.Add(new CngProperty(KeyLengthProperty, BitConverter.GetBytes(KeyBits),
                                              CngPropertyOptions.None));
    CngKey key = CngKey.Create(CngAlgorithm.Rsa, keyName: null, parameters);
    File.WriteAllBytes(keyFile, key.Export(s_privateBlob));
    return new RSACng(key);
  }

  // Every file under `root`, named by its path relative to it with '/'.
  private static byte[] ZipFolder(string root) {
    using var output = new MemoryStream();
    using (var archive = new ZipArchive(output, ZipArchiveMode.Create, leaveOpen: true)) {
      foreach (string file in Directory.EnumerateFiles(root, "*", SearchOption.AllDirectories)) {
        string name = Path.GetRelativePath(root, file).Replace('\\', '/');
        ZipArchiveEntry entry = archive.CreateEntry(name);
        using Stream to = entry.Open();
        using FileStream from = File.OpenRead(file);
        from.CopyTo(to);
      }
    }
    return output.ToArray();
  }

  private static byte[] LittleEndian(uint value) {
    byte[] bytes = new byte[sizeof(uint)];
    BinaryPrimitives.WriteUInt32LittleEndian(bytes, value);
    return bytes;
  }
}
