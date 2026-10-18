# Dump EVERY symbol of a msedge.dll public PDB as "rva<TAB>decorated name" (dbghelp,
# native). Needs only the PDB, not the image, so an old build can be dumped after
# Edge has replaced its dll. port_offsets.py diffs two such dumps.
#   python tools/re/dump_symbols.py <msedge.dll.pdb> <out.tsv>
import ctypes as C
import ctypes.wintypes as W
import sys

PDB = sys.argv[1]
OUT = sys.argv[2]
BASE = 0x180000000
MAX_SYM_NAME = 4000
IMAGE_SPAN = 0x20000000  # larger than msedge.dll's SizeOfImage; dbghelp wants a size
SYMOPT_DEFERRED_LOADS = 0x00000004


class SYMBOL_INFO(C.Structure):
    _fields_ = [("SizeOfStruct", W.ULONG), ("TypeIndex", W.ULONG),
                ("Reserved", C.c_ulonglong * 2), ("Index", W.ULONG), ("Size", W.ULONG),
                ("ModBase", C.c_ulonglong), ("Flags", W.ULONG), ("Value", C.c_ulonglong),
                ("Address", C.c_ulonglong), ("Register", W.ULONG), ("Scope", W.ULONG),
                ("Tag", W.ULONG), ("NameLen", W.ULONG), ("MaxNameLen", W.ULONG),
                ("Name", C.c_char * MAX_SYM_NAME)]


dbg = C.WinDLL("dbghelp.dll")
proc = C.c_void_p(C.WinDLL("kernel32.dll").GetCurrentProcess() or -1)
dbg.SymSetOptions(0)  # raw decorated names
dbg.SymInitialize.argtypes = [C.c_void_p, C.c_char_p, W.BOOL]
if not dbg.SymInitialize(proc, None, False):
    raise OSError("SymInitialize", C.GetLastError())
dbg.SymLoadModuleEx.restype = C.c_ulonglong
dbg.SymLoadModuleEx.argtypes = [C.c_void_p, C.c_void_p, C.c_char_p, C.c_char_p,
                                C.c_ulonglong, W.DWORD, C.c_void_p, W.DWORD]
if not dbg.SymLoadModuleEx(proc, None, PDB.encode(), b"msedge.dll", BASE, IMAGE_SPAN, None, 0):
    raise OSError("SymLoadModuleEx", C.GetLastError())

CB = C.WINFUNCTYPE(W.BOOL, C.POINTER(SYMBOL_INFO), W.ULONG, C.c_void_p)
count = 0
with open(OUT, "w", encoding="utf-8", newline="\n") as out:
    def on_sym(psym, size, ctx):
        global count
        s = psym.contents
        if s.Address >= BASE:
            out.write(f"{s.Address - BASE:x}\t{s.Size}\t{s.Name.decode('latin1')}\n")
            count += 1
        return True

    cb = CB(on_sym)
    dbg.SymEnumSymbols.argtypes = [C.c_void_p, C.c_ulonglong, C.c_char_p, CB, C.c_void_p]
    dbg.SymEnumSymbols(proc, BASE, b"*", cb, None)
print(count, "symbols", file=sys.stderr)
