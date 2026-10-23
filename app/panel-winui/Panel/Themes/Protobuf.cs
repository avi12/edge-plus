using System;
using System.Collections.Generic;
using System.IO;

namespace EdgePlus;

// The slice of the protobuf wire format a CRX3 header uses: varints and
// length-delimited fields.
internal static class Protobuf {
  public const int LengthDelimited = 2;
  private const int WireTypeBits = 3;
  private const int WireTypeMask = 0x7;
  private const int VarintPayloadBits = 7;
  private const byte VarintPayloadMask = 0x7f;
  private const byte VarintContinues = 0x80;
  private const int MaxVarintShift = 63;

  public readonly record struct Field(ulong Number, int WireType, int Start, int Length);

  public static (ulong Value, int Next) ReadVarint(ReadOnlySpan<byte> data, int offset) {
    ulong value = 0;
    int shift = 0;
    while (true) {
      if (offset >= data.Length || shift > MaxVarintShift) {
        throw new InvalidDataException("truncated varint");
      }
      byte piece = data[offset];
      offset++;
      value |= (ulong)(piece & VarintPayloadMask) << shift;
      bool isLast = (piece & VarintContinues) == 0;
      if (isLast) {
        return (value, offset);
      }
      shift += VarintPayloadBits;
    }
  }

  // Any wire type but length-delimited is refused: nothing a CRX header carries uses one.
  public static List<Field> ReadFields(ReadOnlySpan<byte> data, int start, int end) {
    var fields = new List<Field>();
    int offset = start;
    while (offset < end) {
      (ulong tag, int afterTag) = ReadVarint(data, offset);
      int wireType = (int)(tag & WireTypeMask);
      if (wireType != LengthDelimited) {
        throw new InvalidDataException("unexpected CRX header field");
      }
      (ulong length, int payloadStart) = ReadVarint(data, afterTag);
      bool isInside = length <= (ulong)(end - payloadStart);
      if (!isInside) {
        throw new InvalidDataException("truncated CRX header");
      }
      fields.Add(new Field(tag >> WireTypeBits, wireType, payloadStart, (int)length));
      offset = payloadStart + (int)length;
    }
    return fields;
  }

  public static byte[] Varint(ulong value) {
    var bytes = new List<byte>();
    while (true) {
      byte low = (byte)(value & VarintPayloadMask);
      value >>= VarintPayloadBits;
      if (value == 0) {
        bytes.Add(low);
        return bytes.ToArray();
      }
      bytes.Add((byte)(low | VarintContinues));
    }
  }

  public static byte[] LengthDelimitedField(ulong number, ReadOnlySpan<byte> payload) {
    byte[] tag = Varint((number << WireTypeBits) | LengthDelimited);
    byte[] length = Varint((ulong)payload.Length);
    byte[] field = new byte[tag.Length + length.Length + payload.Length];
    tag.CopyTo(field, 0);
    length.CopyTo(field, tag.Length);
    payload.CopyTo(field.AsSpan(tag.Length + length.Length));
    return field;
  }
}
