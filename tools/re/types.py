# Dump exact struct layouts (member -> offset, size, type) from the public PDB via
# Windows dbghelp SymGetTypeInfo. No third-party install; reads the same cache as
# symbols.py. Usage:
#   python tools/re/types.py "<cache dir | path	o\msedge.dll.pdb>" "views::ProposedLayout" "gfx::Rect" ...
# Prints each requested type's members as "  +0xNN  size  name : typename".
import ctypes as C, ctypes.wintypes as W, sys
from edge_paths import installed_dll

CACHE = sys.argv[1]
DLL   = installed_dll()
NAMES = sys.argv[2:]
BASE  = 0x180000000
MAX_SYM_NAME = 2000

SYMOPT_DEBUG = 0x80000000
SYMOPT_UNDNAME = 0x00000002

class SYMBOL_INFO(C.Structure):
    _fields_ = [("SizeOfStruct", W.ULONG), ("TypeIndex", W.ULONG),
                ("Reserved", C.c_ulonglong * 2), ("Index", W.ULONG), ("Size", W.ULONG),
                ("ModBase", C.c_ulonglong), ("Flags", W.ULONG), ("Value", C.c_ulonglong),
                ("Address", C.c_ulonglong), ("Register", W.ULONG), ("Scope", W.ULONG),
                ("Tag", W.ULONG), ("NameLen", W.ULONG), ("MaxNameLen", W.ULONG),
                ("Name", C.c_char * MAX_SYM_NAME)]

# IMAGEHLP_SYMBOL_TYPE_INFO
TI_GET_SYMNAME   = 1
TI_GET_LENGTH    = 2
TI_GET_TYPE      = 3
TI_GET_TYPEID    = 4
TI_GET_OFFSET    = 6
TI_GET_CHILDRENCOUNT = 13
TI_FINDCHILDREN  = 7
TI_GET_SYMTAG    = 10

dbg = C.WinDLL("dbghelp.dll")
k32 = C.WinDLL("kernel32.dll")
proc = C.c_void_p(k32.GetCurrentProcess() or -1)

dbg.SymSetOptions(SYMOPT_DEBUG | SYMOPT_UNDNAME)
IS_PDB = CACHE.lower().endswith(".pdb")   # a PDB path needs no image (the build may be gone)
dbg.SymInitialize.argtypes = [C.c_void_p, C.c_char_p, W.BOOL]
if not dbg.SymInitialize(proc, None if IS_PDB else CACHE.encode(), False):
    raise OSError("SymInitialize", C.GetLastError())
dbg.SymLoadModuleEx.restype = C.c_ulonglong
dbg.SymLoadModuleEx.argtypes = [C.c_void_p, C.c_void_p, C.c_char_p, C.c_char_p,
                                C.c_ulonglong, W.DWORD, C.c_void_p, W.DWORD]
if IS_PDB:
    mod = dbg.SymLoadModuleEx(proc, None, CACHE.encode(), b"msedge.dll", BASE, 0x20000000, None, 0)
else:
    mod = dbg.SymLoadModuleEx(proc, None, DLL.encode(), None, BASE, 0, None, 0)
if not mod:
    raise OSError("SymLoadModuleEx", C.GetLastError())

dbg.SymGetTypeInfo.argtypes = [C.c_void_p, C.c_ulonglong, W.ULONG, W.ULONG, C.c_void_p]
dbg.SymGetTypeInfo.restype = W.BOOL

def ti(tidx, info, buf):
    return dbg.SymGetTypeInfo(proc, BASE, tidx, info, buf)

def get_u32(tidx, info):
    v = W.ULONG(0)
    return v.value if ti(tidx, info, C.byref(v)) else None

def get_u64(tidx, info):
    v = C.c_ulonglong(0)
    return v.value if ti(tidx, info, C.byref(v)) else None

def get_name(tidx):
    p = C.c_void_p(0)
    if not ti(tidx, TI_GET_SYMNAME, C.byref(p)) or not p.value:
        return None
    s = C.wstring_at(p.value)
    k32.LocalFree(p)
    return s

def typename(tidx):
    # Walk to a printable type name; fall back to tag/length.
    nm = get_name(tidx)
    if nm:
        return nm
    tag = get_u32(tidx, TI_GET_SYMTAG)
    ln = get_u64(tidx, TI_GET_LENGTH)
    # pointer (tag 14) -> "<pointee>*"
    if tag == 14:
        sub = get_u32(tidx, TI_GET_TYPEID)
        if sub is not None:
            return (typename(sub) or "?") + "*"
    return f"tag{tag}(len={ln})"

def find_type(name):
    si = SYMBOL_INFO()
    si.SizeOfStruct = C.sizeof(SYMBOL_INFO) - MAX_SYM_NAME + 1
    si.MaxNameLen = MAX_SYM_NAME
    dbg.SymGetTypeFromName.argtypes = [C.c_void_p, C.c_ulonglong, C.c_char_p, C.c_void_p]
    if dbg.SymGetTypeFromName(proc, BASE, name.encode(), C.byref(si)):
        return si.TypeIndex
    return None

class FINDCHILDREN(C.Structure):
    _fields_ = [("Count", W.ULONG), ("Start", W.ULONG), ("ChildId", W.ULONG * 4096)]

def dump(name):
    tidx = find_type(name)
    print(f"\n== {name} ==")
    if tidx is None:
        print("  (type not found)")
        return
    length = get_u64(tidx, TI_GET_LENGTH)
    print(f"  sizeof = {length} (0x{length:x})" if length is not None else "  sizeof = ?")
    count = get_u32(tidx, TI_GET_CHILDRENCOUNT) or 0
    fc = FINDCHILDREN()
    fc.Count = count
    fc.Start = 0
    if count and ti(tidx, TI_FINDCHILDREN, C.byref(fc)):
        for i in range(count):
            cid = fc.ChildId[i]
            tag = get_u32(cid, TI_GET_SYMTAG)
            if tag != 7:   # SymTagData (member); skip methods/bases/etc
                continue
            off = get_u32(cid, TI_GET_OFFSET)
            nm = get_name(cid)
            mt = get_u32(cid, TI_GET_TYPEID)
            tn = typename(mt) if mt is not None else "?"
            sz = get_u64(mt, TI_GET_LENGTH) if mt is not None else None
            szs = f"{sz}" if sz is not None else "?"
            if off is None:
                continue
            print(f"  +0x{off:<4x} {szs:>4}  {nm} : {tn}")

for n in NAMES:
    dump(n)
