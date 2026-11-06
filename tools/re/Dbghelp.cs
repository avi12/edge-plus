// A thin dbghelp wrapper for the tools/re scripts, loaded by EdgePlus.Re.psm1 with
// Add-Type. It reads the public PDB of msedge.dll: symbols by mask, by name and by
// address, and the member layout of a type.
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace EdgePlus.Re {
  public sealed class Symbol {
    public ulong Rva;
    public uint Size;
    public string Name;
  }

  public sealed class Member {
    public uint Offset;
    public ulong Size;
    public string Name;
    public string TypeName;
  }

  public sealed class Dbghelp : IDisposable {
    public const ulong ImageBase = 0x180000000;
    // Larger than msedge.dll's SizeOfImage; dbghelp wants a size when only a PDB is loaded.
    const uint PdbOnlyImageSpan = 0x20000000;
    const int MaxNameLength = 4000;
    const int SymbolInfoSize = 88;
    const int NameOffset = 84;
    const int TypeIndexOffset = 4;
    const int SizeOffset = 28;
    const int AddressOffset = 56;
    const int NameLengthOffset = 76;
    const int MaxNameLengthOffset = 80;
    const uint SymOptUndecorate = 0x2;
    const uint SymOptDebug = 0x80000000;
    const uint UndecorateComplete = 0x1000;

    const uint TypeInfoSymName = 1;
    const uint TypeInfoLength = 2;
    const uint TypeInfoTypeId = 4;
    const uint TypeInfoOffset = 6;
    const uint TypeInfoFindChildren = 7;
    const uint TypeInfoSymTag = 10;
    const uint TypeInfoChildrenCount = 13;
    const uint SymTagData = 7;
    const uint SymTagPointer = 14;

    readonly IntPtr process = GetCurrentProcess();

    delegate bool SymEnumProc(IntPtr symbol, uint size, IntPtr context);

    [DllImport("dbghelp.dll", SetLastError = true)]
    static extern uint SymSetOptions(uint options);
    [DllImport("dbghelp.dll", SetLastError = true, CharSet = CharSet.Ansi)]
    static extern bool SymInitialize(IntPtr process, string searchPath, bool invadeProcess);
    [DllImport("dbghelp.dll", SetLastError = true, CharSet = CharSet.Ansi)]
    static extern ulong SymLoadModuleEx(IntPtr process, IntPtr file, string image, string module,
      ulong baseOfDll, uint size, IntPtr data, uint flags);
    [DllImport("dbghelp.dll", SetLastError = true)]
    static extern bool SymCleanup(IntPtr process);
    [DllImport("dbghelp.dll", SetLastError = true, CharSet = CharSet.Ansi)]
    static extern bool SymEnumSymbols(IntPtr process, ulong baseOfDll, string mask, SymEnumProc callback, IntPtr context);
    [DllImport("dbghelp.dll", SetLastError = true)]
    static extern bool SymFromAddr(IntPtr process, ulong address, out ulong displacement, IntPtr symbol);
    [DllImport("dbghelp.dll", SetLastError = true, CharSet = CharSet.Ansi)]
    static extern bool SymFromName(IntPtr process, string name, IntPtr symbol);
    [DllImport("dbghelp.dll", SetLastError = true, CharSet = CharSet.Ansi)]
    static extern bool SymGetTypeFromName(IntPtr process, ulong baseOfDll, string name, IntPtr symbol);
    [DllImport("dbghelp.dll", SetLastError = true)]
    static extern bool SymGetTypeInfo(IntPtr process, ulong baseOfDll, uint typeId, uint info, IntPtr result);
    [DllImport("dbghelp.dll", CharSet = CharSet.Ansi)]
    static extern uint UnDecorateSymbolName(string name, StringBuilder output, uint maxLength, uint flags);
    [DllImport("kernel32.dll")]
    static extern IntPtr GetCurrentProcess();
    [DllImport("kernel32.dll")]
    static extern IntPtr LocalFree(IntPtr memory);

    // A PDB path loads with no image (the build may be gone); otherwise the image is
    // loaded and symbols come from the cache folder. isDecorated keeps raw names.
    public Dbghelp(string pdbOrImage, string cacheFolder, bool isDecorated) {
      SymSetOptions((isDecorated ? 0u : SymOptUndecorate) | SymOptDebug);
      bool isPdb = pdbOrImage.EndsWith(".pdb", StringComparison.OrdinalIgnoreCase);
      if (!SymInitialize(process, isPdb ? null : cacheFolder, false)) {
        throw new InvalidOperationException("SymInitialize failed: " + Marshal.GetLastWin32Error());
      }
      ulong loaded = isPdb
        ? SymLoadModuleEx(process, IntPtr.Zero, pdbOrImage, "msedge.dll", ImageBase, PdbOnlyImageSpan, IntPtr.Zero, 0)
        : SymLoadModuleEx(process, IntPtr.Zero, pdbOrImage, null, ImageBase, 0, IntPtr.Zero, 0);
      if (loaded == 0) {
        throw new InvalidOperationException("SymLoadModuleEx failed: " + Marshal.GetLastWin32Error());
      }
    }

    public void Dispose() {
      SymCleanup(process);
    }

    static IntPtr NewSymbolBuffer() {
      int length = NameOffset + MaxNameLength;
      IntPtr buffer = Marshal.AllocHGlobal(length);
      for (int offset = 0; offset < length; offset++) {
        Marshal.WriteByte(buffer, offset, 0);
      }
      Marshal.WriteInt32(buffer, 0, SymbolInfoSize);
      Marshal.WriteInt32(buffer, MaxNameLengthOffset, MaxNameLength);
      return buffer;
    }

    static Symbol ReadSymbol(IntPtr buffer) {
      int nameLength = Marshal.ReadInt32(buffer, NameLengthOffset);
      return new Symbol {
        Rva = (ulong)Marshal.ReadInt64(buffer, AddressOffset) - ImageBase,
        Size = (uint)Marshal.ReadInt32(buffer, SizeOffset),
        Name = Marshal.PtrToStringAnsi(buffer + NameOffset, nameLength),
      };
    }

    // Every symbol matching the mask ("*" for all), at or above the image base.
    public List<Symbol> Enumerate(string mask) {
      var found = new List<Symbol>();
      SymEnumProc callback = (symbol, size, context) => {
        ulong address = (ulong)Marshal.ReadInt64(symbol, AddressOffset);
        if (address >= ImageBase) {
          found.Add(ReadSymbol(symbol));
        }
        return true;
      };
      SymEnumSymbols(process, ImageBase, mask, callback, IntPtr.Zero);
      GC.KeepAlive(callback);
      return found;
    }

    public Symbol FromName(string name) {
      IntPtr buffer = NewSymbolBuffer();
      try {
        return SymFromName(process, name, buffer) ? ReadSymbol(buffer) : null;
      } finally {
        Marshal.FreeHGlobal(buffer);
      }
    }

    // The symbol covering the rva, with the displacement into it.
    public Symbol FromRva(ulong rva, out ulong displacement) {
      IntPtr buffer = NewSymbolBuffer();
      try {
        return SymFromAddr(process, ImageBase + rva, out displacement, buffer) ? ReadSymbol(buffer) : null;
      } finally {
        Marshal.FreeHGlobal(buffer);
      }
    }

    public static string Undecorate(string name) {
      if (!name.StartsWith("?")) {
        return name;
      }
      var output = new StringBuilder(1024);
      UnDecorateSymbolName(name, output, 1024, UndecorateComplete);
      return output.Length > 0 ? output.ToString() : name;
    }

    uint? ReadUInt(uint typeId, uint info) {
      IntPtr result = Marshal.AllocHGlobal(8);
      try {
        Marshal.WriteInt64(result, 0);
        return SymGetTypeInfo(process, ImageBase, typeId, info, result) ? (uint)Marshal.ReadInt32(result) : (uint?)null;
      } finally {
        Marshal.FreeHGlobal(result);
      }
    }

    ulong? ReadULong(uint typeId, uint info) {
      IntPtr result = Marshal.AllocHGlobal(8);
      try {
        Marshal.WriteInt64(result, 0);
        return SymGetTypeInfo(process, ImageBase, typeId, info, result) ? (ulong)Marshal.ReadInt64(result) : (ulong?)null;
      } finally {
        Marshal.FreeHGlobal(result);
      }
    }

    string ReadName(uint typeId) {
      IntPtr result = Marshal.AllocHGlobal(IntPtr.Size);
      try {
        Marshal.WriteIntPtr(result, IntPtr.Zero);
        if (!SymGetTypeInfo(process, ImageBase, typeId, TypeInfoSymName, result)) {
          return null;
        }
        IntPtr text = Marshal.ReadIntPtr(result);
        if (text == IntPtr.Zero) {
          return null;
        }
        string name = Marshal.PtrToStringUni(text);
        LocalFree(text);
        return name;
      } finally {
        Marshal.FreeHGlobal(result);
      }
    }

    string TypeName(uint typeId) {
      string name = ReadName(typeId);
      if (name != null) {
        return name;
      }
      uint? tag = ReadUInt(typeId, TypeInfoSymTag);
      if (tag == SymTagPointer) {
        uint? pointee = ReadUInt(typeId, TypeInfoTypeId);
        if (pointee.HasValue) {
          return (TypeName(pointee.Value) ?? "?") + "*";
        }
      }
      return "tag" + tag + "(len=" + ReadULong(typeId, TypeInfoLength) + ")";
    }

    // The data members of a type with their offsets; null when the PDB has no such type.
    public List<Member> Members(string typeName, out ulong? length) {
      length = null;
      IntPtr buffer = NewSymbolBuffer();
      uint typeId;
      try {
        if (!SymGetTypeFromName(process, ImageBase, typeName, buffer)) {
          return null;
        }
        typeId = (uint)Marshal.ReadInt32(buffer, TypeIndexOffset);
      } finally {
        Marshal.FreeHGlobal(buffer);
      }
      length = ReadULong(typeId, TypeInfoLength);
      var members = new List<Member>();
      uint count = ReadUInt(typeId, TypeInfoChildrenCount) ?? 0;
      if (count == 0) {
        return members;
      }
      IntPtr children = Marshal.AllocHGlobal(8 + 4 * (int)count);
      try {
        Marshal.WriteInt32(children, 0, (int)count);
        Marshal.WriteInt32(children, 4, 0);
        if (!SymGetTypeInfo(process, ImageBase, typeId, TypeInfoFindChildren, children)) {
          return members;
        }
        for (int index = 0; index < count; index++) {
          uint childId = (uint)Marshal.ReadInt32(children, 8 + 4 * index);
          if (ReadUInt(childId, TypeInfoSymTag) != SymTagData) {
            continue;
          }
          uint? offset = ReadUInt(childId, TypeInfoOffset);
          if (!offset.HasValue) {
            continue;
          }
          uint? memberType = ReadUInt(childId, TypeInfoTypeId);
          members.Add(new Member {
            Offset = offset.Value,
            Name = ReadName(childId),
            TypeName = memberType.HasValue ? TypeName(memberType.Value) : "?",
            Size = memberType.HasValue ? ReadULong(memberType.Value, TypeInfoLength) ?? 0 : 0,
          });
        }
      } finally {
        Marshal.FreeHGlobal(children);
      }
      return members;
    }
  }
}
