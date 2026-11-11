// The byte work of the signature table generator (New-SignatureTable.ps1), loaded by
// EdgePlus.Signatures.psm1 with Add-Type next to the Iced decoder: a read-only view of
// msedge.dll on disk, masked signatures of its functions, the scan that grows one
// until nothing else in .text matches it, and the RIP-relative and call references
// into a set of addresses.
//
// msedge.dll's .text holds no base relocation at all (every address in code is
// RIP-relative), so the only bytes of a function that move from build to build are
// the 32-bit displacements of calls, jumps and RIP-relative operands. A signature is
// the function's opening bytes with exactly those wildcarded, grown until nothing
// else in the image matches it. The DLL's scanner (src/edge/signature.h) matches
// what this writes.
using System;
using System.Collections.Generic;
using System.IO;
using System.Text;
using Iced.Intel;
using Decoder = Iced.Intel.Decoder;

namespace EdgePlus.Re {
  // How much of a function a mask throws away.
  public enum SignatureLevel {
    // Only what a rebuild moves: rel32 branch targets and RIP-relative displacements.
    RelocationAware = 1,
    // Also every memory displacement and 32/64-bit immediate: tolerates a struct that
    // grew a field, at the price of a less specific pattern.
    Tolerant = 2,
  }

  public sealed class PeSection {
    const uint ExecutableFlag = 0x20000000;

    public string Name;
    public uint VirtualAddress;
    public uint VirtualSize;
    public uint RawOffset;
    public uint RawSize;
    public uint Flags;

    public bool IsExecutable {
      get { return (Flags & ExecutableFlag) != 0; }
    }

    public bool Contains(uint rva) {
      return VirtualAddress <= rva && rva < (ulong)VirtualAddress + Math.Min(VirtualSize, RawSize);
    }
  }

  // A PE image on disk, read rather than loaded: RVA -> bytes without laying the
  // whole image out the way the loader does.
  public sealed class PeImage {
    const int PePointerOffset = 0x3C;
    const int SectionCountOffset = 6;
    const int OptionalHeaderSizeOffset = 20;
    const int OptionalHeaderOffset = 24;
    const int DataDirectoriesOffset = 112;
    const int DataDirectorySize = 8;
    const int SectionHeaderSize = 40;
    const int SectionNameLength = 8;
    const int SectionSizesOffset = 8;
    const int SectionFlagsOffset = 36;
    const int ExceptionDirectoryIndex = 3;
    const int RuntimeFunctionSize = 12;
    const string TextSectionName = ".text";

    public readonly string Path;
    public readonly byte[] File;
    public readonly List<PeSection> Sections = new List<PeSection>();
    public readonly PeSection Text;
    // The exception table (.pdata): the image's own list of function entry points and
    // ends, the same on every build and needing no PDB. Sorted by start.
    public readonly uint[] FunctionStarts;
    public readonly uint[] FunctionEnds;
    public readonly HashSet<uint> FunctionStartSet;

    public PeImage(string path) {
      Path = path;
      File = System.IO.File.ReadAllBytes(path);
      int peOffset = BitConverter.ToInt32(File, PePointerOffset);
      int sectionCount = BitConverter.ToUInt16(File, peOffset + SectionCountOffset);
      int optionalSize = BitConverter.ToUInt16(File, peOffset + OptionalHeaderSizeOffset);
      int optional = peOffset + OptionalHeaderOffset;
      int table = optional + optionalSize;
      for (int index = 0; index < sectionCount; index++) {
        int at = table + index * SectionHeaderSize;
        string name = Encoding.ASCII.GetString(File, at, SectionNameLength).TrimEnd('\0');
        Sections.Add(new PeSection {
          Name = name,
          VirtualSize = BitConverter.ToUInt32(File, at + SectionSizesOffset),
          VirtualAddress = BitConverter.ToUInt32(File, at + SectionSizesOffset + 4),
          RawSize = BitConverter.ToUInt32(File, at + SectionSizesOffset + 8),
          RawOffset = BitConverter.ToUInt32(File, at + SectionSizesOffset + 12),
          Flags = BitConverter.ToUInt32(File, at + SectionFlagsOffset),
        });
      }
      Text = Sections.Find(section => section.Name == TextSectionName);
      int exceptionDirectory = optional + DataDirectoriesOffset + ExceptionDirectoryIndex * DataDirectorySize;
      uint exceptionRva = BitConverter.ToUInt32(File, exceptionDirectory);
      uint exceptionSize = BitConverter.ToUInt32(File, exceptionDirectory + 4);
      long exceptionOffset = OffsetOf(exceptionRva);
      int count = (int)(exceptionSize / RuntimeFunctionSize);
      FunctionStarts = new uint[count];
      FunctionEnds = new uint[count];
      for (int index = 0; index < count; index++) {
        int at = (int)exceptionOffset + index * RuntimeFunctionSize;
        FunctionStarts[index] = BitConverter.ToUInt32(File, at);
        FunctionEnds[index] = BitConverter.ToUInt32(File, at + 4);
      }
      FunctionStartSet = new HashSet<uint>(FunctionStarts);
    }

    public PeSection SectionOf(uint rva) {
      return Sections.Find(section => section.Contains(rva));
    }

    public bool IsCode(uint rva) {
      PeSection section = SectionOf(rva);
      return section != null && section.IsExecutable;
    }

    // The file offset of an RVA; -1 when no section's raw data holds it.
    public long OffsetOf(uint rva) {
      PeSection section = SectionOf(rva);
      if (section == null) {
        return -1;
      }
      return (long)section.RawOffset + rva - section.VirtualAddress;
    }

    // Up to `length` bytes at rva, cut at the end of its section's raw data; empty
    // when no section holds it.
    public byte[] Read(uint rva, int length) {
      PeSection section = SectionOf(rva);
      if (section == null) {
        return new byte[0];
      }
      long offset = (long)section.RawOffset + rva - section.VirtualAddress;
      long end = Math.Min(offset + length, (long)section.RawOffset + section.RawSize);
      byte[] bytes = new byte[Math.Max(0, end - offset)];
      Array.Copy(File, offset, bytes, 0, bytes.Length);
      return bytes;
    }

    // The index of the function whose unwind range holds rva; -1 when none does.
    int FunctionContaining(uint rva) {
      int low = 0;
      int high = FunctionStarts.Length;
      while (low < high) {
        int middle = (low + high) / 2;
        if (rva < FunctionStarts[middle]) {
          high = middle;
        } else {
          low = middle + 1;
        }
      }
      int index = low - 1;
      bool isOutsideEveryFunction = index < 0 || rva >= FunctionEnds[index];
      if (isOutsideEveryFunction) {
        return -1;
      }
      return index;
    }

    // The start of the function whose unwind range holds rva; null when none does.
    public uint? FunctionStart(uint rva) {
      int index = FunctionContaining(rva);
      if (index < 0) {
        return null;
      }
      return FunctionStarts[index];
    }

    // The end of the function whose unwind range holds rva; null when none does.
    public uint? FunctionEnd(uint rva) {
      int index = FunctionContaining(rva);
      if (index < 0) {
        return null;
      }
      return FunctionEnds[index];
    }
  }

  public sealed class MaskedSignature {
    public const byte Keep = 0xFF;
    public const byte Wildcard = 0x00;

    public readonly byte[] Pattern;
    public readonly byte[] Mask;  // Keep = the byte must match, Wildcard = any byte

    public MaskedSignature(byte[] pattern, byte[] mask) {
      Pattern = pattern;
      Mask = mask;
    }

    public int Length {
      get { return Pattern.Length; }
    }

    // "48 8B ?? 05 ...": the form signatures.g.h stores and signature.h parses.
    public string Text() {
      var parts = new string[Pattern.Length];
      for (int index = 0; index < Pattern.Length; index++) {
        parts[index] = Mask[index] == Wildcard ? "??" : Pattern[index].ToString("X2");
      }
      return string.Join(" ", parts);
    }

    // The first of the longest runs of bytes that must match: what a scan looks for.
    public void LongestLiteralRun(out int bestStart, out int bestLength) {
      bestStart = 0;
      bestLength = 0;
      int start = -1;
      for (int index = 0; index < Mask.Length; index++) {
        if (Mask[index] == Wildcard) {
          start = -1;
          continue;
        }
        if (start < 0) {
          start = index;
        }
        int length = index - start + 1;
        if (length > bestLength) {
          bestStart = start;
          bestLength = length;
        }
      }
    }

    public MaskedSignature Prefix(int length) {
      int kept = Math.Min(length, Pattern.Length);
      var pattern = new byte[kept];
      var mask = new byte[kept];
      Array.Copy(Pattern, pattern, kept);
      Array.Copy(Mask, mask, kept);
      return new MaskedSignature(pattern, mask);
    }
  }

  // What the Iced decoder says about the opening of a piece of code.
  public static class SignatureDecoder {
    public const int MinimumLiteralRun = 4;
    const int DisplacementSize = 4;
    const int BitnessOf64 = 64;
    // Bytes decoded past a limit, so the instruction that crosses it is whole.
    public const int InstructionSlack = 16;

    // One decoded instruction and where its displacement and immediate sit in it
    // (Iced hands those out only from the decoder that just decoded it).
    sealed class Decoded {
      public Instruction Instruction;
      public ConstantOffsets Offsets;
    }

    // Every instruction of the first `length` bytes of `code` (decoded at address 0)
    // up to the first that does not decode or is cut off.
    static IEnumerable<Decoded> Decode(byte[] code, int length) {
      int usable = Math.Min(length, code.Length);
      var bytes = new byte[usable];
      Array.Copy(code, bytes, usable);
      Decoder decoder = Decoder.Create(BitnessOf64, new ByteArrayCodeReader(bytes), 0);
      while (decoder.IP < (ulong)usable) {
        decoder.Decode(out Instruction instruction);
        if (instruction.IsInvalid) {
          yield break;
        }
        yield return new Decoded { Instruction = instruction, Offsets = decoder.GetConstantOffsets(instruction) };
      }
    }

    static bool HasNearBranchOperand(in Instruction instruction) {
      for (int index = 0; index < instruction.OpCount; index++) {
        OpKind kind = instruction.GetOpKind(index);
        bool isNearBranch = kind == OpKind.NearBranch16 || kind == OpKind.NearBranch32 || kind == OpKind.NearBranch64;
        if (isNearBranch) {
          return true;
        }
      }
      return false;
    }

    // The (offset, length) byte ranges of one instruction that a rebuild may move.
    static List<KeyValuePair<int, int>> Wildcards(Decoded decoded, SignatureLevel level) {
      var ranges = new List<KeyValuePair<int, int>>();
      Instruction instruction = decoded.Instruction;
      ConstantOffsets offsets = decoded.Offsets;
      if (instruction.IsIPRelativeMemoryOperand) {
        ranges.Add(new KeyValuePair<int, int>(offsets.DisplacementOffset, DisplacementSize));
      } else {
        // Iced reports a branch's displacement as its immediate, as capstone does.
        bool isRelativeBranch32 = HasNearBranchOperand(instruction) && offsets.ImmediateSize == DisplacementSize;
        if (isRelativeBranch32) {
          ranges.Add(new KeyValuePair<int, int>(offsets.ImmediateOffset, DisplacementSize));
        }
      }
      bool isTolerantOfEveryConstant = level >= SignatureLevel.Tolerant && ranges.Count == 0;
      if (isTolerantOfEveryConstant) {
        if (offsets.HasDisplacement) {
          ranges.Add(new KeyValuePair<int, int>(offsets.DisplacementOffset, offsets.DisplacementSize));
        }
        if (offsets.ImmediateSize >= DisplacementSize) {
          ranges.Add(new KeyValuePair<int, int>(offsets.ImmediateOffset, offsets.ImmediateSize));
        }
      }
      return ranges;
    }

    // How many bytes of `code` to keep as its opening: up to `limit`, but ending at
    // the first return or jump, since what follows belongs to the next function.
    public static int OpeningLength(byte[] code, int limit) {
      int length = 0;
      foreach (Decoded decoded in Decode(code, limit + InstructionSlack)) {
        Instruction instruction = decoded.Instruction;
        if (length >= limit) {
          break;
        }
        length += instruction.Length;
        bool isEndOfFlow = instruction.FlowControl == FlowControl.Return || instruction.Mnemonic == Mnemonic.Jmp;
        if (isEndOfFlow) {
          break;
        }
      }
      return length;
    }

    // The first instructions of `code`, up to maximumLength bytes, with the moving
    // bytes wildcarded; null when the opening does not decode.
    public static MaskedSignature MaskedBytes(byte[] code, SignatureLevel level, int maximumLength) {
      var pattern = new List<byte>();
      var mask = new List<byte>();
      foreach (Decoded decoded in Decode(code, maximumLength + InstructionSlack)) {
        Instruction instruction = decoded.Instruction;
        if (pattern.Count >= maximumLength) {
          break;
        }
        int start = (int)instruction.IP;
        var keep = new byte[instruction.Length];
        for (int index = 0; index < keep.Length; index++) {
          keep[index] = MaskedSignature.Keep;
        }
        foreach (KeyValuePair<int, int> range in Wildcards(decoded, level)) {
          int rangeEnd = Math.Min(range.Key + range.Value, keep.Length);
          for (int index = range.Key; index < rangeEnd; index++) {
            keep[index] = MaskedSignature.Wildcard;
          }
        }
        for (int index = 0; index < keep.Length; index++) {
          pattern.Add(code[start + index]);
          mask.Add(keep[index]);
        }
      }
      if (pattern.Count < MinimumLiteralRun) {
        return null;
      }
      return new MaskedSignature(pattern.ToArray(), mask.ToArray());
    }

    // Where in a whole function's `code` the bytes a rebuild moves sit (the strict
    // mask: rel32 branch targets and RIP-relative displacements), as the offset of
    // each 4-byte field. Decoding stops at the first byte that is not an
    // instruction; what follows stays literal, on both sides of the comparison.
    public static List<int> BodyMaskOffsets(byte[] code) {
      var offsets = new List<int>();
      foreach (Decoded decoded in Decode(code, code.Length)) {
        int start = (int)decoded.Instruction.IP;
        foreach (KeyValuePair<int, int> range in Wildcards(decoded, SignatureLevel.RelocationAware)) {
          bool isWithinCode = start + range.Key + range.Value <= code.Length;
          if (isWithinCode) {
            offsets.Add(start + range.Key);
          }
        }
      }
      return offsets;
    }

    const ulong FnvOffsetBasis = 14695981039346656037UL;
    const ulong FnvPrime = 1099511628211UL;

    // FNV-1a over `code` with the bytes at each mask offset (4 each) taken as
    // zero: the fingerprint resolve.h computes the same way over the running build.
    public static ulong BodyHash(byte[] code, List<int> maskOffsets) {
      var masked = (byte[])code.Clone();
      foreach (int offset in maskOffsets) {
        for (int index = 0; index < DisplacementSize; index++) {
          masked[offset + index] = 0;
        }
      }
      ulong hash = FnvOffsetBasis;
      foreach (byte value in masked) {
        hash ^= value;
        hash *= FnvPrime;
      }
      return hash;
    }

    // The offset of the first instruction of `code` that starts at or after
    // `wanted`; -1 when the code runs out (or stops decoding) before one does.
    public static int InstructionAtOrAfter(byte[] code, int wanted) {
      foreach (Decoded decoded in Decode(code, code.Length)) {
        Instruction instruction = decoded.Instruction;
        int position = (int)instruction.IP;
        if (position >= wanted) {
          return position;
        }
      }
      return -1;
    }
  }

  // The shortest prefix of a signature that matches its own code and nothing else
  // but `Ties` (other places matching all of it).
  public sealed class UniquePrefix {
    public MaskedSignature Signature;
    public List<uint> Ties;
  }

  // Finds masked patterns in one image's .text.
  public sealed class SignatureScanner {
    public const int MinimumLength = 16;

    readonly byte[] file;
    readonly uint baseRva;
    readonly long begin;
    readonly long end;

    public SignatureScanner(PeImage image) {
      file = image.File;
      baseRva = image.Text.VirtualAddress;
      begin = image.Text.RawOffset;
      end = (long)image.Text.RawOffset + image.Text.RawSize;
    }

    // Every file offset in .text where the literal run starts, minus runStart: the
    // places the signature could open at, each at or after the start of .text.
    IEnumerable<long> PrefixCandidates(byte[] run, int runStart) {
      long position = begin;
      while (position + run.Length <= end) {
        int found = new ReadOnlySpan<byte>(file, (int)position, (int)(end - position)).IndexOf(run);
        if (found < 0) {
          yield break;
        }
        long start = position + found - runStart;
        position += found + 1;
        if (begin <= start) {
          yield return start;
        }
      }
    }

    // Null when the signature has no literal run long enough to scan for.
    public UniquePrefix ShortestUnique(MaskedSignature signature, uint ownRva) {
      signature.LongestLiteralRun(out int runStart, out int runLength);
      if (runLength < SignatureDecoder.MinimumLiteralRun) {
        return null;
      }
      var run = new byte[runLength];
      Array.Copy(signature.Pattern, runStart, run, 0, runLength);
      int fullLength = signature.Length;
      int required = runStart + runLength;
      var ties = new List<uint>();
      foreach (long start in PrefixCandidates(run, runStart)) {
        uint hit = (uint)(start - begin + baseRva);
        if (hit == ownRva) {
          continue;
        }
        if (start + fullLength > end) {
          continue;
        }
        int firstDifference = FirstDifference(signature, start);
        if (firstDifference < 0) {
          ties.Add(hit);
          continue;
        }
        required = Math.Max(required, firstDifference + 1);
      }
      return new UniquePrefix {
        Signature = signature.Prefix(Math.Max(required, MinimumLength)),
        Ties = ties,
      };
    }

    // The first byte of the signature the file at `start` does not match; -1 when it
    // matches all of it.
    int FirstDifference(MaskedSignature signature, long start) {
      for (int index = 0; index < signature.Length; index++) {
        bool isDifferent = ((file[start + index] ^ signature.Pattern[index]) & signature.Mask[index]) != 0;
        if (isDifferent) {
          return index;
        }
      }
      return -1;
    }

    // The RVAs of every match of the signature in .text, the first `limit` of them
    // (0 = all); none when it has no literal run long enough to scan for.
    public List<uint> FindAll(MaskedSignature signature, int limit) {
      var hits = new List<uint>();
      signature.LongestLiteralRun(out int runStart, out int runLength);
      if (runLength < SignatureDecoder.MinimumLiteralRun) {
        return hits;
      }
      var run = new byte[runLength];
      Array.Copy(signature.Pattern, runStart, run, 0, runLength);
      foreach (long start in PrefixCandidates(run, runStart)) {
        if (start + signature.Length > end) {
          continue;
        }
        if (FirstDifference(signature, start) >= 0) {
          continue;
        }
        hits.Add((uint)(start - begin + baseRva));
        bool isAtLimit = limit > 0 && hits.Count >= limit;
        if (isAtLimit) {
          break;
        }
      }
      return hits;
    }
  }

  // A signature and how it picks one function out of the places it matches: the
  // function is the `Ordinal`-th (by address) of exactly `Total` matches.
  public sealed class Located {
    public MaskedSignature Signature;
    public int Ordinal;
    public int Total;
    public uint FunctionSize;

    public bool IsUnique {
      get { return Total == 1; }
    }

    // The address this signature stands for among `hits`; null unless there are
    // exactly Total of them.
    public uint? Pick(List<uint> hits) {
      if (hits.Count != Total) {
        return null;
      }
      var sorted = new List<uint>(hits);
      sorted.Sort();
      return sorted[Ordinal];
    }
  }

  // A probe of an entry: a located signature `Offset` bytes into the entry's function.
  public sealed class PlacedProbe {
    public int Offset;
    public Located Located;
  }

  // A RIP-relative displacement in .text and the immediate bytes that end its
  // instruction after it.
  public sealed class RipReference : IComparable<RipReference> {
    public uint DisplacementRva;
    public int Extra;

    public int CompareTo(RipReference other) {
      int byRva = DisplacementRva.CompareTo(other.DisplacementRva);
      if (byRva != 0) {
        return byRva;
      }
      return Extra.CompareTo(other.Extra);
    }
  }

  // Raw-byte scans of .text for references into a set of addresses, the way a
  // rebuild keeps them: a ModRM byte of RIP-relative form followed by its 32-bit
  // displacement, and the rel32 of a call or jmp. No decoding: a hit is a candidate
  // the caller still has to find a function around.
  public static class CodeReferences {
    const byte ModRmFormMask = 0xC7;
    const byte RipRelativeForm = 0x05;
    // The fewest bytes before a ModRM byte and the bytes kept free after it.
    const int ModRmLead = 3;
    const int RipTail = 8;
    const int DisplacementSize = 4;
    const byte CallOpcode = 0xE8;
    const byte JumpOpcode = 0xE9;
    const int CallLength = 5;
    // Bytes of immediate that may follow a RIP-relative displacement.
    static readonly int[] RipFormExtras = { 0, 1, 4 };

    static int Displacement(byte[] file, long at) {
      return BitConverter.ToInt32(file, (int)at);
    }

    // {target: references}: every place in .text where a RIP-relative displacement
    // addresses one of `targets`.
    public static Dictionary<uint, List<RipReference>> RipReferences(PeImage image, HashSet<uint> targets) {
      var found = new Dictionary<uint, List<RipReference>>();
      byte[] file = image.File;
      long textStart = image.Text.RawOffset;
      long textSize = image.Text.RawSize;
      long baseRva = image.Text.VirtualAddress;
      foreach (int extra in RipFormExtras) {
        for (long index = 0; index < textSize - ModRmLead - RipTail; index++) {
          bool isRipRelativeModRm = (file[textStart + ModRmLead + index] & ModRmFormMask) == RipRelativeForm;
          if (!isRipRelativeModRm) {
            continue;
          }
          long position = index + ModRmLead + 1;
          long destination = baseRva + position + DisplacementSize + extra + Displacement(file, textStart + position);
          bool isWanted = destination >= 0 && destination <= uint.MaxValue && targets.Contains((uint)destination);
          if (!isWanted) {
            continue;
          }
          if (!found.TryGetValue((uint)destination, out List<RipReference> list)) {
            list = new List<RipReference>();
            found[(uint)destination] = list;
          }
          list.Add(new RipReference { DisplacementRva = (uint)(baseRva + position), Extra = extra });
        }
      }
      return found;
    }

    // {target: displacement RVAs}: every `call rel32` / `jmp rel32` in .text that
    // lands on one of `targets`; the displacement is the 4 bytes after the opcode.
    public static Dictionary<uint, List<uint>> CallReferences(PeImage image, HashSet<uint> targets) {
      var found = new Dictionary<uint, List<uint>>();
      byte[] file = image.File;
      long textStart = image.Text.RawOffset;
      long textSize = image.Text.RawSize;
      long baseRva = image.Text.VirtualAddress;
      for (long index = 0; index < textSize - CallLength; index++) {
        byte opcode = file[textStart + index];
        bool isCallOrJump = opcode == CallOpcode || opcode == JumpOpcode;
        if (!isCallOrJump) {
          continue;
        }
        long position = index + 1;
        long destination = baseRva + position + DisplacementSize + Displacement(file, textStart + position);
        bool isWanted = destination >= 0 && destination <= uint.MaxValue && targets.Contains((uint)destination);
        if (!isWanted) {
          continue;
        }
        if (!found.TryGetValue((uint)destination, out List<uint> list)) {
          list = new List<uint>();
          found[(uint)destination] = list;
        }
        list.Add((uint)(baseRva + position));
      }
      return found;
    }
  }
}
