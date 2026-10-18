# Enumerate msedge.dll symbols from the downloaded public PDB via Windows dbghelp.
# Prints "RVA\tname" for symbols matching the split-view classes, so Frida can hook
# module.base.add(rva) by name. Native dbghelp — no third-party install.
import ctypes as C, ctypes.wintypes as W, sys, os
from edge_paths import installed_dll

CACHE = sys.argv[1] if len(sys.argv) > 1 else r"."   # dir holding msedge.dll.pdb
DLL   = installed_dll()
MASKS = sys.argv[2:] or ["*SplitTab*", "*MultiContents*", "*SplitWindow*", "*split_window*"]

MAX_SYM_NAME = 2000
SYMOPT_UNDNAME = 0x00000002
SYMOPT_LOAD_LINES = 0x00000010
SYMOPT_DEBUG = 0x80000000
BASE = 0x180000000

class SYMBOL_INFO(C.Structure):
    _fields_ = [("SizeOfStruct", W.ULONG), ("TypeIndex", W.ULONG),
                ("Reserved", C.c_ulonglong * 2), ("Index", W.ULONG), ("Size", W.ULONG),
                ("ModBase", C.c_ulonglong), ("Flags", W.ULONG), ("Value", C.c_ulonglong),
                ("Address", C.c_ulonglong), ("Register", W.ULONG), ("Scope", W.ULONG),
                ("Tag", W.ULONG), ("NameLen", W.ULONG), ("MaxNameLen", W.ULONG),
                ("Name", C.c_char * MAX_SYM_NAME)]

dbg = C.WinDLL("dbghelp.dll")
k32 = C.WinDLL("kernel32.dll")
proc = k32.GetCurrentProcess()
proc = C.c_void_p(proc if proc else -1)

dbg.SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEBUG | SYMOPT_LOAD_LINES)
dbg.SymInitialize.argtypes = [C.c_void_p, C.c_char_p, W.BOOL]
if not dbg.SymInitialize(proc, CACHE.encode(), False):
    raise OSError("SymInitialize failed", C.GetLastError())

dbg.SymLoadModuleExW  # presence check not required
dbg.SymLoadModuleEx.restype = C.c_ulonglong
dbg.SymLoadModuleEx.argtypes = [C.c_void_p, C.c_void_p, C.c_char_p, C.c_char_p,
                                C.c_ulonglong, W.DWORD, C.c_void_p, W.DWORD]
mod = dbg.SymLoadModuleEx(proc, None, DLL.encode(), None, BASE, 0, None, 0)
if not mod:
    raise OSError("SymLoadModuleEx failed", C.GetLastError())
print("loaded module base", hex(mod), file=sys.stderr)

CB = C.WINFUNCTYPE(W.BOOL, C.POINTER(SYMBOL_INFO), W.ULONG, C.c_void_p)
rows = []
def on_sym(psym, size, ctx):
    s = psym.contents
    rows.append((s.Address - BASE, s.Name.decode("latin1", "replace")))
    return True
cb = CB(on_sym)

dbg.SymEnumSymbols.argtypes = [C.c_void_p, C.c_ulonglong, C.c_char_p, CB, C.c_void_p]
for mask in MASKS:
    dbg.SymEnumSymbols(proc, BASE, mask.encode(), cb, None)

seen = set()
for rva, name in sorted(rows):
    if (rva, name) in seen:
        continue
    seen.add((rva, name))
    print(f"{rva:#010x}\t{name}")
print(f"\n{len(seen)} symbols", file=sys.stderr)
