using System;
using System.Linq;
using System.Security.Cryptography;

namespace EdgePlus;

// An extension's id: 32 characters of a..p, the first 16 bytes of the SHA-256
// of its public key with each nibble spelled as a letter. One home for the
// shape, read by the store-link parser, the CRX reader, the packer and the
// installed-theme scan alike.
internal static class ExtensionId {
  public const int Length = 32;
  private const int HashBytes = Length / 2;
  private const string Alphabet = "abcdefghijklmnop";
  private const int NibbleBits = 4;
  private const int NibbleMask = 0xf;

  public static bool IsValid(string value) =>
      value.Length == Length && value.All(letter => letter >= Alphabet[0] && letter <= Alphabet[^1]);

  public static string FromPublicKey(ReadOnlySpan<byte> publicKey) {
    byte[] hash = SHA256.HashData(publicKey);
    var id = new char[Length];
    for (int i = 0; i < HashBytes; i++) {
      id[i * 2] = Alphabet[hash[i] >> NibbleBits];
      id[i * 2 + 1] = Alphabet[hash[i] & NibbleMask];
    }
    return new string(id);
  }
}
