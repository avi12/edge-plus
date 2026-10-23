using System;
using System.Buffers.Binary;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text;
using System.Text.Json;

namespace EdgePlus;

internal sealed record CrxInfo(string Id, string Version);

internal sealed record CrxProof(byte[] PublicKey, byte[] Signature, byte[] SignedHeaderData);

// The fields of CrxFileHeader, of its AsymmetricKeyProof and of its
// SignedData, as protobuf field numbers.
internal enum CrxHeaderField : ulong {
  RsaProof = 2,
  SignedHeaderData = 10_000,
}

internal enum KeyProofField : ulong {
  PublicKey = 1,
  Signature = 2,
}

internal enum SignedDataField : ulong {
  CrxId = 1,
}

// A CRX3 file (read here, written by ThemePacker) is a "Cr24" magic, the format
// version, the header's length, a CrxFileHeader protobuf carrying the RSA
// public key, then a ZIP. The extension id is derived from that public key; the
// version is the "version" of the ZIP's manifest.json.
internal static class Crx {
  public static readonly byte[] Magic = "Cr24"u8.ToArray();
  public const uint FormatVersion = 3;
  // The magic, the format version and the header length, each four bytes.
  public const int PreambleLength = 12;
  // What every CRX3 signature covers, ahead of the signed header data.
  public static readonly byte[] SignedDataPrefix = "CRX3 SignedData\0"u8.ToArray();

  private const int VersionOffset = 4;
  private const int HeaderLengthOffset = 8;

  public static CrxInfo Read(byte[] bytes) {
    CrxProof proof = ReadProof(bytes);
    return new CrxInfo(ExtensionId.FromPublicKey(proof.PublicKey), ReadZipVersion(Zip(bytes)));
  }

  // Only the first RSA proof is read.
  public static CrxProof ReadProof(byte[] bytes) {
    int headerEnd = PreambleLength + HeaderLength(bytes);
    var header = Protobuf.ReadFields(bytes, PreambleLength, headerEnd);
    Protobuf.Field proofField = header.FirstOrDefault(field => field.Is(CrxHeaderField.RsaProof));
    if (proofField.Length == 0) {
      throw new InvalidDataException("no public key in CRX header");
    }
    var proof = Protobuf.ReadFields(bytes, proofField.Start, proofField.Start + proofField.Length);
    Protobuf.Field key = proof.FirstOrDefault(field => field.Is(KeyProofField.PublicKey));
    if (key.Length == 0) {
      throw new InvalidDataException("no public key in key proof");
    }
    Protobuf.Field signature = proof.FirstOrDefault(field => field.Is(KeyProofField.Signature));
    Protobuf.Field signed =
        header.FirstOrDefault(field => field.Is(CrxHeaderField.SignedHeaderData));
    return new CrxProof(Slice(bytes, key), Slice(bytes, signature), Slice(bytes, signed));
  }

  public static byte[] Zip(byte[] bytes) => bytes[(PreambleLength + HeaderLength(bytes))..];

  public static string ReadZipVersion(byte[] zipBytes) {
    using var archive = OpenZip(zipBytes, "bad .crx ZIP");
    ZipArchiveEntry manifest = archive.GetEntry(ThemeManifest.ManifestName)
        ?? throw new InvalidDataException("no manifest.json in .crx");
    using var reader = new StreamReader(manifest.Open(), Encoding.UTF8);
    try {
      using JsonDocument parsed = JsonDocument.Parse(reader.ReadToEnd());
      bool hasVersion = parsed.RootElement.ValueKind == JsonValueKind.Object
          && parsed.RootElement.TryGetProperty(ManifestKeys.Version, out JsonElement version)
          && version.ValueKind == JsonValueKind.String;
      if (!hasVersion) {
        throw new InvalidDataException("no version in manifest.json");
      }
      return parsed.RootElement.GetProperty(ManifestKeys.Version).GetString()!;
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
