using System;
using System.Buffers.Binary;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Text.Json;

namespace EdgePlus;

// A local theme's identity, read from its .crx.
internal sealed record CrxInfo(string Id, string Version);

// The RSA proof a CRX3 header carries, and the signed header data beside it.
internal sealed record CrxProof(byte[] PublicKey, byte[] Signature, byte[] SignedHeaderData);

// The CRX3 format, both what ThemePacker writes and what Crx reads.
//
// A CRX3 file is a "Cr24" magic, the format version, the header's length, a
// CrxFileHeader protobuf carrying the RSA public key, then a ZIP. The
// extension id is derived from that public key (ExtensionId); the version is
// the "version" of the ZIP's manifest.json.
internal static class Crx {
  public static readonly byte[] Magic = "Cr24"u8.ToArray();
  public const uint FormatVersion = 3;
  // The magic, the format version and the header length, each four bytes.
  public const int PreambleLength = 12;
  // The fields of CrxFileHeader and of its AsymmetricKeyProof.
  public const ulong RsaProofField = 2;
  public const ulong SignedHeaderDataField = 10_000;
  public const ulong PublicKeyField = 1;
  public const ulong SignatureField = 2;
  public const ulong CrxIdField = 1;
  // What every CRX3 signature covers, ahead of the signed header data.
  public static readonly byte[] SignedDataPrefix = "CRX3 SignedData\0"u8.ToArray();

  private const int VersionOffset = 4;
  private const int HeaderLengthOffset = 8;
  private const string ManifestName = "manifest.json";
  private const string VersionKey = "version";

  // The extension id and version of CRX3 `bytes`.
  public static CrxInfo Read(byte[] bytes) {
    CrxProof proof = ReadProof(bytes);
    return new CrxInfo(ExtensionId.FromPublicKey(proof.PublicKey), ReadZipVersion(Zip(bytes)));
  }

  // The first RSA proof and the signed header data of CRX3 `bytes`.
  public static CrxProof ReadProof(byte[] bytes) {
    int headerEnd = PreambleLength + HeaderLength(bytes);
    var header = Protobuf.ReadFields(bytes, PreambleLength, headerEnd);
    Protobuf.Field proofField = header.FirstOrDefault(field => field.Number == RsaProofField);
    if (proofField.Length == 0) {
      throw new InvalidDataException("no public key in CRX header");
    }
    var proof = Protobuf.ReadFields(bytes, proofField.Start, proofField.Start + proofField.Length);
    Protobuf.Field key = proof.FirstOrDefault(field => field.Number == PublicKeyField);
    if (key.Length == 0) {
      throw new InvalidDataException("no public key in key proof");
    }
    Protobuf.Field signature = proof.FirstOrDefault(field => field.Number == SignatureField);
    Protobuf.Field signed = header.FirstOrDefault(field => field.Number == SignedHeaderDataField);
    return new CrxProof(Slice(bytes, key), Slice(bytes, signature), Slice(bytes, signed));
  }

  // The ZIP a CRX3 carries after its header.
  public static byte[] Zip(byte[] bytes) => bytes[(PreambleLength + HeaderLength(bytes))..];

  // The "version" in the manifest.json of ZIP `zipBytes`.
  public static string ReadZipVersion(byte[] zipBytes) {
    using var archive = OpenZip(zipBytes, "bad .crx ZIP");
    ZipArchiveEntry manifest = archive.GetEntry(ManifestName)
        ?? throw new InvalidDataException("no manifest.json in .crx");
    using var reader = new StreamReader(manifest.Open(), Encoding.UTF8);
    try {
      using JsonDocument parsed = JsonDocument.Parse(reader.ReadToEnd());
      bool hasVersion = parsed.RootElement.ValueKind == JsonValueKind.Object
          && parsed.RootElement.TryGetProperty(VersionKey, out JsonElement version)
          && version.ValueKind == JsonValueKind.String;
      if (!hasVersion) {
        throw new InvalidDataException("no version in manifest.json");
      }
      return parsed.RootElement.GetProperty(VersionKey).GetString()!;
    } catch (JsonException) {
      throw new InvalidDataException("bad manifest.json");
    }
  }

  private static ZipArchive OpenZip(byte[] zipBytes, string failure) {
    try {
      return new ZipArchive(new MemoryStream(zipBytes), ZipArchiveMode.Read);
    } catch (InvalidDataException) {
      throw new InvalidDataException(failure);
    }
  }

  private static int HeaderLength(byte[] bytes) {
    bool isCrx3 = bytes.Length > PreambleLength
        && bytes.AsSpan(0, Magic.Length).SequenceEqual(Magic)
        && BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(VersionOffset)) == FormatVersion;
    if (!isCrx3) {
      throw new InvalidDataException("not a CRX3 file");
    }
    uint length = BinaryPrimitives.ReadUInt32LittleEndian(bytes.AsSpan(HeaderLengthOffset));
    bool isInside = length <= (uint)(bytes.Length - PreambleLength);
    if (!isInside) {
      throw new InvalidDataException("truncated CRX header");
    }
    return (int)length;
  }

  private static byte[] Slice(byte[] bytes, Protobuf.Field field) =>
      bytes.AsSpan(field.Start, field.Length).ToArray();
}
