# Port app/native-split/src/edge/offsets.h from one msedge.dll build to the next.
#
#   python tools/re/port_offsets.py --old-pdb OLD.pdb --new-pdb NEW.pdb \
#       --new-dll "<...>\msedge.dll" [--old-dll OLD.dll] \
#       [--table docs/re/port-<build>.md] [--apply]
#
# Needs only the two public PDBs (tools/re/dump_symbols.py turns each into a
# "rva size name" listing, cached next to the PDB as <pdb>.tsv). For every RVA in the
# `rva` / `addr` namespaces of offsets.h it
#   1. finds the symbol(s) at the old RVA, looks each name up in the new PDB
#      (method "symbol", or "symbol+rank" when the name is shared by several symbols);
#   2. else (a patch site inside a function) finds the containing old function and
#      carries the displacement over (method "func+disp");
#   3. checks the result: a function's size must match its old size (or is flagged),
#      a func+disp site must land on an instruction boundary of the new image, and
#      with --old-dll the opening instructions of both functions must have the same
#      mnemonics (a "shape" check).
# Anything it cannot place with confidence is "unresolved" and is left unchanged in
# --apply mode, but the pin is NOT bumped while any entry is unresolved unless
# --allow-unresolved is given. Hex literals inside offsets.h comments that name an
# old RVA are listed so they can be fixed by hand.
import argparse
import bisect
import collections
import os
import re
import subprocess
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
IMAGE_BASE = 0x180000000
MAX_OWNER_EXTENT = 0x100000
ENTRY = re.compile(
    r"(inline constexpr uintptr_t\s+)(\w+)(\s*(?:\[\])?\s*=\s*)((?:kImageBase\s*\+\s*)?)"
    r"(0x[0-9a-fA-F]+)(;)", re.S)
ARRAY = re.compile(
    r"(inline constexpr uintptr_t\s+)(\w+)\[\]\s*=\s*\{([^}]*)\};", re.S)
HEX = re.compile(r"0x[0-9a-fA-F]+")


def load_symbols(pdb_or_tsv):
    tsv = pdb_or_tsv if pdb_or_tsv.endswith(".tsv") else pdb_or_tsv + ".tsv"
    if not os.path.exists(tsv):
        subprocess.check_call([sys.executable, os.path.join(HERE, "dump_symbols.py"),
                               pdb_or_tsv, tsv])
    rows = []
    with open(tsv, encoding="utf-8") as handle:
        for line in handle:
            rva, size, name = line.rstrip("\n").split("\t", 2)
            rows.append((int(rva, 16), int(size), name))
    return rows


class SymbolIndex:
    def __init__(self, rows):
        self.rows = sorted(rows)
        self.starts = [row[0] for row in self.rows]
        self.by_name = collections.defaultdict(list)
        self.at = collections.defaultdict(list)
        for rva, size, name in self.rows:
            self.by_name[name].append((rva, size))
            self.at[rva].append((size, name))
        for name in self.by_name:
            self.by_name[name].sort()
        self.distinct = sorted(set(self.starts))

    def containing(self, rva):
        # The nearest symbol start <= rva whose extent (size, or the gap to the
        # next symbol when the PDB records none) covers it.
        i = bisect.bisect_right(self.distinct, rva) - 1
        if i < 0:
            return None
        start = self.distinct[i]
        size = max(s for s, _ in self.at[start])
        end = start + size if size else (self.distinct[i + 1] if i + 1 < len(self.distinct) else start + 1)
        # A symbol-less region (cold code) would otherwise be "owned" by whatever
        # symbol precedes it, megabytes away: no owner.
        return (start, size, end) if rva < end and end - start <= MAX_OWNER_EXTENT else None

    def extent(self, start):
        size = max(s for s, _ in self.at[start])
        if size:
            return size
        i = bisect.bisect_right(self.distinct, start)
        return (self.distinct[i] - start) if i < len(self.distinct) else 0


class Image:
    def __init__(self, path):
        import pefile
        self.pe = pefile.PE(path, fast_load=True)
        self.data = self.pe.get_memory_mapped_image()

    def read(self, rva, length):
        return self.data[rva:rva + length]


def instruction_starts(image, start, length):
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    code = image.read(start, length)
    return [insn.address for insn in md.disasm(code, start)]


def shape(image, start, count=12):
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    return [insn.mnemonic for _, insn in zip(range(count), md.disasm(image.read(start, 96), start))]


def undecorate(name):
    import ctypes
    if not name.startswith("?"):
        return name
    buffer = ctypes.create_string_buffer(1024)
    ctypes.WinDLL("dbghelp.dll").UnDecorateSymbolName(name.encode(), buffer, 1024, 0x1000)
    return buffer.value.decode("latin1") or name


def place(old_rva, old, new, new_image, old_image):
    """Return (new_rva or None, method, confidence, note)."""
    names = old.at.get(old_rva, [])
    votes = collections.Counter()
    notes = []
    for size, name in names:
        old_group = old.by_name[name]
        new_group = new.by_name.get(name)
        if not new_group:
            notes.append(f"{name[:60]}: gone")
            continue
        same_size_old = [entry for entry in old_group if entry[1] == size]
        same_size_new = [entry for entry in new_group if entry[1] == size]
        if len(old_group) == 1 and len(new_group) == 1:
            candidate, new_size = new_group[0]
            votes[(candidate, "symbol", size == new_size)] += 1
        elif len(same_size_old) == 1 and len(same_size_new) == 1 and size:
            # An overloaded / folded name: its size picks the same function.
            votes[(same_size_new[0][0], "symbol+size", True)] += 1
        elif len(old_group) == len(new_group):
            rank = old_group.index((old_rva, size))
            candidate, new_size = new_group[rank]
            votes[(candidate, "symbol+rank", size == new_size)] += 1
        else:
            notes.append(f"{name[:60]}: {len(old_group)} before, {len(new_group)} now")
    if votes:
        (candidate, kind, same_size), count = votes.most_common(1)[0]
        agree = len({v[0] for v in votes}) == 1
        if not agree:
            return None, kind, "none", "names disagree: " + ", ".join(hex(v[0]) for v in votes)
        if kind in ("symbol", "symbol+size") and same_size:
            confidence = "high"
        elif kind == "symbol":
            confidence = "medium"
            notes.append("size changed")
        else:
            confidence = "medium" if same_size else "low"
        old_extent = max(size for size, _ in names)
        new_extent = max(size for size, _ in new.at[candidate])
        if old_extent and new_extent and old_extent != new_extent:
            # The name still exists but the body grew or shrank: the code a hook or
            # a struct offset was reversed from has changed. Re-verify by hand.
            confidence = "low"
            notes.append(f"size {old_extent}->{new_extent}")
        if old_image is not None and new_image is not None and not names[0][0] == 0:
            if shape(old_image, old_rva) != shape(new_image, candidate):
                confidence = "low"
                notes.append("opening instructions differ")
        return candidate, kind, confidence, "; ".join(notes)
    if not names:
        owner = old.containing(old_rva)
        if owner is not None:
            start, size, end = owner
            displacement = old_rva - start
            owner_names = [n for _, n in old.at[start]]
            sub, kind, confidence, note = place(start, old, new, new_image, old_image)
            if sub is not None:
                new_size = new.extent(sub)
                old_size = old.extent(start)
                if old_size != new_size:
                    confidence = "low"
                    note += f"; owner size {old_size}->{new_size}"
                elif confidence == "high":
                    confidence = "medium"
                if new_image is not None:
                    starts = instruction_starts(new_image, sub, displacement + 16)
                    if sub + displacement not in starts:
                        # A patched immediate or displacement byte: legitimate, but
                        # only the DLL's own byte check can vouch for it.
                        note += "; inside an instruction (patched operand)"
                        confidence = "low"
                return sub + displacement, "func+disp", confidence, f"in {owner_names[0][:50]} +{displacement:#x} {note}"
            return None, "func+disp", "none", f"owner {owner_names[0][:50]}: {note}"
    return None, "none", "none", "; ".join(notes) or "no symbol"


def parse_entries(text):
    """(name, old_value, is_addr) for every scalar and array entry of rva + addr."""
    start = text.index("namespace rva {")
    end = text.index("namespace off {")
    section = text[start:end]
    for match in ARRAY.finditer(section):
        for position, hexvalue in enumerate(HEX.findall(match.group(3))):
            yield f"{match.group(2)}[{position}]", int(hexvalue, 16), False
    for match in ENTRY.finditer(section):
        yield match.group(2), int(match.group(5), 16), bool(match.group(4))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--offsets", default=os.path.join(HERE, "..", "..", "app", "native-split", "src", "offsets.h"))
    ap.add_argument("--old-pdb", required=True)
    ap.add_argument("--new-pdb", required=True)
    ap.add_argument("--new-dll", required=True)
    ap.add_argument("--old-dll")
    ap.add_argument("--table", help="write the old->new review table (markdown)")
    ap.add_argument("--apply", action="store_true", help="rewrite offsets.h in place")
    ap.add_argument("--allow-unresolved", action="store_true")
    ap.add_argument("--override", action="append", default=[], metavar="NAME=0xNEW:why",
                    help="a hand-verified RVA for an entry the PDB cannot place "
                         "(e.g. a patch site in symbol-less cold code)")
    args = ap.parse_args()

    with open(args.offsets, encoding="utf-8") as handle:
        text = handle.read()
    old = SymbolIndex(load_symbols(args.old_pdb))
    new = SymbolIndex(load_symbols(args.new_pdb))
    new_image = Image(args.new_dll)
    old_image = Image(args.old_dll) if args.old_dll and os.path.exists(args.old_dll) else None

    overrides = {}
    for spec in args.override:
        name, _, rest = spec.partition("=")
        value, _, why = rest.partition(":")
        overrides[name] = (int(value, 16), why)
    rows = []
    for name, old_value, is_addr in parse_entries(text):
        old_rva = old_value
        if name in overrides:
            new_rva, note = overrides[name]
            method, confidence = "manual", "verified"
        else:
            new_rva, method, confidence, note = place(old_rva, old, new, new_image, old_image)
        label = ""
        for _, sym in old.at.get(old_rva, [])[:1]:
            label = undecorate(sym)
        rows.append((name, old_rva, new_rva, method, confidence, label, note, is_addr))

    tally = collections.Counter()
    for row in rows:
        tally[(row[3] if row[2] is not None else "unresolved", row[4])] += 1
    print(f"{len(rows)} entries")
    for key, count in sorted(tally.items()):
        print(f"  {key[0]:12} {key[1]:7} {count}")
    unresolved = [row for row in rows if row[2] is None]
    for row in unresolved:
        print(f"  UNRESOLVED {row[0]} {row[1]:#x}: {row[6]}")

    if args.table:
        with open(args.table, "w", encoding="utf-8", newline="\n") as out:
            out.write("| entry | old RVA | new RVA | delta | method | confidence | symbol / note |\n")
            out.write("|---|---|---|---|---|---|---|\n")
            for name, old_rva, new_rva, method, confidence, label, note, _ in rows:
                shown = f"{new_rva:#x}" if new_rva is not None else "UNRESOLVED"
                delta = f"{new_rva - old_rva:+#x}" if new_rva is not None else ""
                detail = (label[:90] + ("; " + note if note else "")).replace("|", "\\|")
                out.write(f"| {name} | {old_rva:#x} | {shown} | {delta} | {method} | {confidence} | {detail} |\n")

    if args.apply:
        if unresolved and not args.allow_unresolved:
            sys.exit("refusing to apply with unresolved entries (use --allow-unresolved)")
        mapping = {}
        for name, old_rva, new_rva, _, _, _, _, is_addr in rows:
            if new_rva is not None:
                mapping[old_rva] = new_rva

        start = text.index("namespace rva {")
        end = text.index("namespace off {")
        section = text[start:end]

        def replace_scalar(match):
            is_addr = bool(match.group(4))
            value = int(match.group(5), 16)
            old_rva = value
            if old_rva not in mapping:
                return match.group(0)
            new_rva = mapping[old_rva]
            digits = max(len(match.group(5)) - 2, 0)
            literal = f"0x{new_rva:0{digits}x}"
            return match.group(1) + match.group(2) + match.group(3) + match.group(4) + literal + match.group(6)

        def replace_array(match):
            def one(hexmatch):
                value = int(hexmatch.group(0), 16)
                return f"0x{mapping[value]:x}" if value in mapping else hexmatch.group(0)
            return match.group(1) + match.group(2) + "[] = {" + HEX.sub(one, match.group(3)) + "};"

        section = ARRAY.sub(replace_array, section)
        section = ENTRY.sub(replace_scalar, section)
        text = text[:start] + section + text[end:]
        with open(args.offsets, "w", encoding="utf-8", newline="\n") as handle:
            handle.write(text)
        print("offsets.h rewritten (clang-format it; bump kVersion* and the panel pin by hand)")

    # Old RVAs quoted in comments are not rewritten; list them for a manual fix.
    for lineno, line in enumerate(text.splitlines(), 1):
        if "//" not in line:
            continue
        comment = line.split("//", 1)[1]
        for literal in HEX.findall(comment):
            value = int(literal, 16)
            if value in old.at and value > 0x10000:
                print(f"  comment line {lineno}: {literal} is an old RVA ({undecorate(old.at[value][0][1])[:60]})")


if __name__ == "__main__":
    main()
