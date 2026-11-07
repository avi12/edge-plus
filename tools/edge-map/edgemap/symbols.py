# A public PDB's symbols as "rva size name" rows (tools/re/dump_symbols.py makes the
# .tsv once per PDB), looked up by exact name. LTO renames some symbols
# ".weak.<name>.default.ld-temp.o" next to the plain one; both count as <name>.
import collections
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DUMP = os.path.join(HERE, "..", "..", "re", "dump_symbols.py")
WEAK = re.compile(r"^\.weak\.(.*)\.default\.[^.]+\.o$")


def canonical(name):
    match = WEAK.match(name)
    return match.group(1) if match else name


class Symbols:
    def __init__(self, pdb_path):
        tsv = pdb_path if pdb_path.endswith(".tsv") else pdb_path + ".tsv"
        if not os.path.exists(tsv):
            subprocess.check_call([sys.executable, DUMP, pdb_path, tsv])
        self.by_name = collections.defaultdict(set)
        self.sizes = {}
        starts = set()
        with open(tsv, encoding="utf-8") as handle:
            for line in handle:
                rva, size, name = line.rstrip("\n").split("\t", 2)
                rva = int(rva, 16)
                size = int(size)
                self.by_name[canonical(name)].add(rva)
                self.sizes[rva] = max(self.sizes.get(rva, 0), size)
                starts.add(rva)
        self.starts = sorted(starts)

    def addresses(self, name):
        return sorted(self.by_name.get(name, ()))

    def address(self, name):
        found = self.addresses(name)
        if not found:
            raise LookupError(f"no symbol {name}")
        if len(found) > 1:
            raise LookupError(f"{name} names {len(found)} addresses: "
                              + ", ".join(f"{rva:#x}" for rva in found))
        return found[0]

    def matching(self, pattern):
        """Every name matching a regex, with its addresses (for globbed lists)."""
        regex = re.compile(pattern)
        return {name: sorted(rvas) for name, rvas in self.by_name.items() if regex.fullmatch(name)}

    def extent(self, rva, limit):
        """How far the code at `rva` reaches: its recorded size, else up to the
        next symbol, never past `limit` bytes."""
        size = self.sizes.get(rva, 0)
        if size:
            return min(size, limit)
        import bisect
        index = bisect.bisect_right(self.starts, rva)
        following = self.starts[index] if index < len(self.starts) else rva + limit
        return min(following - rva, limit)
