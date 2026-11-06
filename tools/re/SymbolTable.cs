// The symbols of one msedge.dll build, read from the "rva size name" listing that
// Export-Symbols.ps1 writes, indexed by name and by address. Port-Offsets.ps1 asks it
// where a name moved and which function owns an address.
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;

namespace EdgePlus.Re {
  public sealed class Placement {
    public ulong Rva;
    public ulong Size;
  }

  public sealed class NamedSize {
    public ulong Size;
    public string Name;
  }

  public sealed class Owner {
    public ulong Start;
    public ulong Size;
    public ulong End;
  }

  public sealed class SymbolTable {
    // A symbol-less region (cold code) would otherwise be "owned" by whatever symbol
    // precedes it, megabytes away: no owner.
    const ulong MaxOwnerExtent = 0x100000;

    readonly Dictionary<string, List<Placement>> byName = new Dictionary<string, List<Placement>>();
    readonly Dictionary<ulong, List<NamedSize>> at = new Dictionary<ulong, List<NamedSize>>();
    readonly ulong[] starts;

    public SymbolTable(string tsvPath) {
      var distinct = new SortedSet<ulong>();
      foreach (string line in File.ReadLines(tsvPath)) {
        string[] fields = line.Split(new[] { '\t' }, 3);
        ulong rva = Convert.ToUInt64(fields[0], 16);
        ulong size = ulong.Parse(fields[1]);
        string name = fields[2];
        List<Placement> sameName;
        if (!byName.TryGetValue(name, out sameName)) {
          sameName = new List<Placement>();
          byName[name] = sameName;
        }
        sameName.Add(new Placement { Rva = rva, Size = size });
        List<NamedSize> sameAddress;
        if (!at.TryGetValue(rva, out sameAddress)) {
          sameAddress = new List<NamedSize>();
          at[rva] = sameAddress;
        }
        sameAddress.Add(new NamedSize { Size = size, Name = name });
        distinct.Add(rva);
      }
      foreach (List<Placement> group in byName.Values) {
        group.Sort((left, right) => left.Rva != right.Rva ? left.Rva.CompareTo(right.Rva) : left.Size.CompareTo(right.Size));
      }
      starts = distinct.ToArray();
    }

    // Every symbol of that name, by address; empty when the build has none.
    public IReadOnlyList<Placement> WithName(string name) {
      List<Placement> group;
      return byName.TryGetValue(name, out group) ? group : new List<Placement>();
    }

    // Every symbol starting at the address; empty when none does.
    public IReadOnlyList<NamedSize> StartingAt(ulong rva) {
      List<NamedSize> group;
      return at.TryGetValue(rva, out group) ? group : new List<NamedSize>();
    }

    // The size recorded for a function, else the gap to the next symbol.
    public ulong Extent(ulong start) {
      ulong size = StartingAt(start).Select(entry => entry.Size).DefaultIfEmpty(0UL).Max();
      if (size != 0) {
        return size;
      }
      int next = Array.BinarySearch(starts, start) + 1;
      return next < starts.Length && next > 0 ? starts[next] - start : 0;
    }

    // The nearest symbol start at or below the address whose extent covers it.
    public Owner OwnerOf(ulong rva) {
      int position = Array.BinarySearch(starts, rva);
      if (position < 0) {
        position = ~position - 1;
      }
      if (position < 0) {
        return null;
      }
      ulong start = starts[position];
      ulong size = StartingAt(start).Select(entry => entry.Size).DefaultIfEmpty(0UL).Max();
      ulong end = size != 0 ? start + size : position + 1 < starts.Length ? starts[position + 1] : start + 1;
      bool isOwned = rva < end && end - start <= MaxOwnerExtent;
      return isOwned ? new Owner { Start = start, Size = size, End = end } : null;
    }

    public bool HasSymbolAt(ulong rva) {
      return at.ContainsKey(rva);
    }
  }
}
