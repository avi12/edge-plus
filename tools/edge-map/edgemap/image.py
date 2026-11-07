# A msedge.dll image on disk: its bytes by RVA, its sections, and what identifies
# the build (file version, PE timestamp, SizeOfImage). Read once, never mapped.
import ctypes
import ctypes.wintypes as wintypes
import struct
from dataclasses import dataclass

IMAGE_BASE = 0x180000000


@dataclass(frozen=True)
class Section:
    name: str
    rva: int
    virtual_size: int
    raw_offset: int
    raw_size: int
    is_code: bool


class Image:
    def __init__(self, path):
        self.path = path
        with open(path, "rb") as handle:
            self.data = handle.read()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        section_count = struct.unpack_from("<H", self.data, pe + 6)[0]
        optional_size = struct.unpack_from("<H", self.data, pe + 20)[0]
        optional = pe + 24
        self.timestamp = struct.unpack_from("<I", self.data, pe + 8)[0]
        self.size_of_image = struct.unpack_from("<I", self.data, optional + 56)[0]
        self.preferred_base = struct.unpack_from("<Q", self.data, optional + 24)[0]
        self.sections = []
        for index in range(section_count):
            entry = optional + optional_size + index * 40
            name = self.data[entry:entry + 8].rstrip(b"\0").decode("ascii", "replace")
            virtual_size, rva, raw_size, raw_offset = struct.unpack_from("<IIII", self.data, entry + 8)
            characteristics = struct.unpack_from("<I", self.data, entry + 36)[0]
            is_code = bool(characteristics & 0x20000000)  # IMAGE_SCN_MEM_EXECUTE
            self.sections.append(Section(name, rva, virtual_size, raw_offset, raw_size, is_code))
        self.version = file_version(path)

    def section_of(self, rva):
        for section in self.sections:
            if section.rva <= rva < section.rva + max(section.virtual_size, section.raw_size):
                return section
        return None

    def is_code(self, rva):
        section = self.section_of(rva)
        return section is not None and section.is_code

    def read(self, rva, length):
        section = self.section_of(rva)
        if section is None:
            raise ValueError(f"rva {rva:#x} is in no section")
        offset = section.raw_offset + (rva - section.rva)
        end = min(offset + length, section.raw_offset + section.raw_size)
        chunk = self.data[offset:end]
        # Bytes past the section's raw data are zero once loaded.
        return chunk + b"\0" * (length - len(chunk))

    def u32(self, rva):
        return struct.unpack("<I", self.read(rva, 4))[0]

    def i32(self, rva):
        return struct.unpack("<i", self.read(rva, 4))[0]

    def u64(self, rva):
        return struct.unpack("<Q", self.read(rva, 8))[0]

    def pointer_rva(self, rva):
        """The RVA an absolute pointer stored at `rva` points to (vtables, tables)."""
        value = self.u64(rva)
        return value - self.preferred_base if value >= self.preferred_base else None

    def identity(self):
        return {"version": self.version, "timestamp": self.timestamp,
                "size_of_image": self.size_of_image}


def file_version(path):
    version_dll = ctypes.WinDLL("version.dll")
    size = version_dll.GetFileVersionInfoSizeW(path, None)
    if not size:
        raise OSError(f"no version resource in {path}")
    buffer = ctypes.create_string_buffer(size)
    if not version_dll.GetFileVersionInfoW(path, 0, size, buffer):
        raise OSError(f"cannot read the version of {path}")
    fixed = ctypes.c_void_p()
    length = wintypes.UINT()
    version_dll.VerQueryValueW(buffer, "\\", ctypes.byref(fixed), ctypes.byref(length))
    ms, ls = struct.unpack_from("<II", ctypes.string_at(fixed.value, length.value), 8)
    return f"{ms >> 16}.{ms & 0xFFFF}.{ls >> 16}.{ls & 0xFFFF}"
