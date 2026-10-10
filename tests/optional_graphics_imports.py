# MAINTENANCE: regression-keep. No hardware or third-party Python dependencies.
"""Reject startup imports of optional graphics runtimes in a shipped PE file."""
import pathlib
import struct
import sys


def imported_dlls(path):
    data = path.read_bytes()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[:2] != b"MZ" or data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("not a PE image")
    sections = struct.unpack_from("<H", data, pe + 6)[0]
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    optional = pe + 24
    magic = struct.unpack_from("<H", data, optional)[0]
    if magic not in (0x10B, 0x20B):
        raise ValueError("unknown PE optional header")
    directories = optional + (112 if magic == 0x20B else 96)
    table = optional + optional_size

    def offset(rva):
        for index in range(sections):
            virtual_size, start, raw_size, raw = struct.unpack_from(
                "<IIII", data, table + index * 40 + 8)
            if start <= rva < start + max(virtual_size, raw_size):
                return raw + rva - start
        raise ValueError(f"unmapped RVA {rva:#x}")

    # Both ordinary and delay imports must stay absent. Runtime wrappers handle
    # a missing library explicitly; delay-load exceptions are not that fallback.
    for directory, stride, name_offset in ((1, 20, 12), (13, 32, 4)):
        rva, size = struct.unpack_from("<II", data, directories + directory * 8)
        if not rva:
            continue
        start = offset(rva)
        for descriptor in range(start, start + size, stride):
            if not any(data[descriptor:descriptor + stride]):
                break
            name_rva = struct.unpack_from("<I", data, descriptor + name_offset)[0]
            begin = offset(name_rva)
            yield data[begin:data.index(0, begin)].decode("ascii").lower()


if __name__ == "__main__":
    path = pathlib.Path(sys.argv[1])
    imports = sorted(set(imported_dlls(path)))
    forbidden = {"d3d12.dll", "dxgi.dll", "d3dcompiler_47.dll"}
    bad = sorted(forbidden.intersection(imports))
    print(f"{path}: optional startup imports={bad}")
    sys.exit(1 if bad else 0)
