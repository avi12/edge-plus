# Print the PDB name + GUID/age (symbol-server key) for a PE, from its debug directory.
import struct, sys
DLL = sys.argv[1] if len(sys.argv) > 1 else r"C:\Program Files (x86)\Microsoft\Edge\Application\155.0.4283.45\msedge.dll"
d = open(DLL, "rb").read()
pe = struct.unpack_from("<I", d, 0x3c)[0]
nsec = struct.unpack_from("<H", d, pe+6)[0]; szopt = struct.unpack_from("<H", d, pe+20)[0]
opt = pe+24
debug_rva, debug_size = struct.unpack_from("<II", d, opt+112+6*8)
sects = []
for i in range(nsec):
    o = opt+szopt+i*40
    vsz, va = struct.unpack_from("<II", d, o+8); rsz, rp = struct.unpack_from("<II", d, o+16)
    sects.append((va, vsz, rp, rsz))
def rva_foff(r):
    for va, vsz, rp, rsz in sects:
        if va <= r < va+max(vsz, rsz): return rp+(r-va)
dbg = rva_foff(debug_rva)
for i in range(debug_size//28):
    o = dbg+i*28
    if struct.unpack_from("<I", d, o+12)[0] != 2:  # CODEVIEW
        continue
    cv = struct.unpack_from("<I", d, o+24)[0]
    if d[cv:cv+4] != b"RSDS":
        continue
    g = struct.unpack_from("<IHH8s", d, cv+4)
    age = struct.unpack_from("<I", d, cv+20)[0]
    name = d[cv+24:d.index(b"\0", cv+24)].decode("latin1")
    key = "%08X%04X%04X%s%X" % (g[0], g[1], g[2], g[3].hex().upper(), age)
    print(name, key)
    print(f"https://msdl.microsoft.com/download/symbols/{name}/{key}/{name}")
