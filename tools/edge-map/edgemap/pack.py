# A pack: the values the recipes found in one msedge.dll build, as the DLL reads
# them. Plain text, one line each, so the DLL parses it with no library:
#
#   edge-map 1
#   version 156.0.4314.42
#   image <PE timestamp hex> <SizeOfImage hex>
#   <name> <hex value>
#   <name> list <hex value> [<hex value> ...]
#   <name> -                                  (the recipe found nothing)
#
# Negative numbers are written as two's complement 64-bit.
FORMAT = 1
MISSING = "-"
LIST = "list"
MASK = (1 << 64) - 1


def write(path, identity, values):
    lines = [f"edge-map {FORMAT}",
             f"version {identity['version']}",
             f"image {identity['timestamp']:x} {identity['size_of_image']:x}"]
    for name in sorted(values):
        value = values[name]
        if isinstance(value, Exception) or value is None:
            lines.append(f"{name} {MISSING}")
        elif isinstance(value, list):
            lines.append(f"{name} {LIST} " + " ".join(f"{item & MASK:x}" for item in value))
        else:
            lines.append(f"{name} {value & MASK:x}")
    with open(path, "w", encoding="ascii", newline="\n") as handle:
        handle.write("\n".join(lines) + "\n")


def signed(value):
    return value - (1 << 64) if value & (1 << 63) else value


def read(path):
    identity = {}
    values = {}
    with open(path, encoding="ascii") as handle:
        header = handle.readline().split()
        if header != ["edge-map", str(FORMAT)]:
            raise ValueError(f"{path} is not an edge-map {FORMAT} pack")
        for line in handle:
            parts = line.split()
            if not parts:
                continue
            key, rest = parts[0], parts[1:]
            if key == "version":
                identity["version"] = rest[0]
            elif key == "image":
                identity["timestamp"] = int(rest[0], 16)
                identity["size_of_image"] = int(rest[1], 16)
            elif rest == [MISSING]:
                values[key] = None
            elif rest[0] == LIST:
                values[key] = [signed(int(item, 16)) for item in rest[1:]]
            else:
                values[key] = signed(int(rest[0], 16))
    return identity, values
