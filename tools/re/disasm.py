# Disassemble named msedge.dll functions, resolving call/lea targets to PDB symbol names.
# Answers "where is the split capped at two" by reading the geometry functions directly.
import ctypes as C, ctypes.wintypes as W, os, sys, struct
from capstone import Cs, CS_ARCH_X86, CS_MODE_64, CS_OP_IMM, CS_OP_MEM, x86

CACHE = sys.argv[1]
DLL = r"C:\Program Files (x86)\Microsoft\Edge\Application\155.0.4283.45\msedge.dll"
TARGETS = sys.argv[2:]
BASE = 0x180000000

# ---- map DLL file bytes + RVA<->foff ----
d = open(DLL, "rb").read()
pe = struct.unpack_from("<I", d, 0x3c)[0]
nsec = struct.unpack_from("<H", d, pe+6)[0]; szopt = struct.unpack_from("<H", d, pe+20)[0]
opt = pe+24; sects = []
for i in range(nsec):
    o = opt+szopt+i*40
    vsz, va = struct.unpack_from("<II", d, o+8); rsz, rp = struct.unpack_from("<II", d, o+16)
    sects.append((va, vsz, rp, rsz))
def rva_foff(r):
    for va, vsz, rp, rsz in sects:
        if va <= r < va+max(vsz, rsz):
            o = rp+(r-va)
            return o if o < rp+rsz else None

# ---- dbghelp for name resolution ----
MAX = 2000
class SYM(C.Structure):
    _fields_ = [("SizeOfStruct", W.ULONG), ("TypeIndex", W.ULONG), ("Reserved", C.c_ulonglong*2),
                ("Index", W.ULONG), ("Size", W.ULONG), ("ModBase", C.c_ulonglong), ("Flags", W.ULONG),
                ("Value", C.c_ulonglong), ("Address", C.c_ulonglong), ("Register", W.ULONG),
                ("Scope", W.ULONG), ("Tag", W.ULONG), ("NameLen", W.ULONG), ("MaxNameLen", W.ULONG),
                ("Name", C.c_char*MAX)]
dbg = C.WinDLL("dbghelp.dll"); proc = C.c_void_p(-1)
dbg.SymSetOptions(0x2 | 0x80000000)
dbg.SymInitialize.argtypes = [C.c_void_p, C.c_char_p, W.BOOL]
dbg.SymInitialize(proc, CACHE.encode(), False)
dbg.SymLoadModuleEx.restype = C.c_ulonglong
dbg.SymLoadModuleEx.argtypes = [C.c_void_p, C.c_void_p, C.c_char_p, C.c_char_p, C.c_ulonglong, W.DWORD, C.c_void_p, W.DWORD]
dbg.SymLoadModuleEx(proc, None, DLL.encode(), None, BASE, 0, None, 0)
dbg.SymFromAddr.argtypes = [C.c_void_p, C.c_ulonglong, C.POINTER(C.c_ulonglong), C.POINTER(SYM)]
def name_of(rva):
    buf = SYM(); buf.SizeOfStruct = 88; buf.MaxNameLen = MAX
    disp = C.c_ulonglong(0)
    if dbg.SymFromAddr(proc, BASE+rva, C.byref(disp), C.byref(buf)):
        n = buf.Name.decode("latin1", "replace")
        return n + (f"+{disp.value:#x}" if disp.value else "")
    return None
def addr_of(name):
    buf = SYM(); buf.SizeOfStruct = 88; buf.MaxNameLen = MAX
    dbg.SymFromName.argtypes = [C.c_void_p, C.c_char_p, C.POINTER(SYM)]
    if dbg.SymFromName(proc, name.encode(), C.byref(buf)):
        return buf.Address-BASE, buf.Size
    return None, None

md = Cs(CS_ARCH_X86, CS_MODE_64); md.detail = True
for t in TARGETS:
    rva, size = addr_of(t)
    if rva is None:
        print(f"\n### {t}: not found"); continue
    size = min(size or 0, int(os.environ.get("DISASM_MAX", "500"))) or 400
    fo = rva_foff(rva); code = d[fo:fo+size]
    print(f"\n### {t}  rva {rva:#x}  size {size}")
    for ins in md.disasm(code, BASE+rva):
        tgt = ""
        for op in ins.operands:
            if op.type == CS_OP_IMM and ins.mnemonic in ("call", "jmp") or (op.type == CS_OP_IMM and ins.mnemonic.startswith("j")):
                nm = name_of(op.imm-BASE)
                if nm: tgt = " ; -> "+nm
            if op.type == CS_OP_MEM and op.mem.base == x86.X86_REG_RIP:
                ea = ins.address+ins.size+op.mem.disp
                nm = name_of(ea-BASE)
                if nm and ins.mnemonic in ("call", "jmp", "lea"): tgt = " ; "+nm
        print(f"  {ins.address-BASE:#08x}: {ins.mnemonic:<7}{ins.op_str}{tgt}")
