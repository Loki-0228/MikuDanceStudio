"""Generate a texture-free PMD test grid; no reference model redistribution.

Usage: python tools/make_pmd_perf_fixture.py output.pmd vertex_count
The mixed weights, edge flags and occasional invalid bone exercise the skin
kernel's existing guards. This synthetic fixture is not a real-world FPS test.
"""
import pathlib
import struct
import sys


def generate(path, count):
    if not 3 <= count <= 65535 or count % 3:
        raise ValueError("vertex_count must be a multiple of 3 in [3,65535]")
    pack = struct.pack
    data = bytearray(b"Pmd" + pack("<f", 1.0))
    data += b"perf-grid".ljust(20, b"\0") + bytes(256)
    data += pack("<I", count)
    for i in range(count):
        data += pack("<8f2H2B", (i % 101) * .1, (i // 101) * .02,
                     (i % 3) * .03, 0., 0., 1., (i % 11) * .1, .5,
                     65535 if i % 97 == 0 else 0, 1, i % 101, i % 2)
    data += pack("<I", count) + pack("<" + "H" * count, *range(count))
    data += pack("<I", 1)
    data += pack("<11f2BI20s", .7, .7, .7, 1., 8., 0., 0., 0., .3, .3, .3,
                 255, 1, count, bytes(20))
    data += pack("<H", 2)
    for i in range(2):
        data += pack("<20sHHBH3f", ("bone%d" % i).encode(),
                     65535 if i == 0 else 0, 1 if i == 0 else 65535,
                     0, 0, 0., float(i), 0.)
    data += pack("<HHBBIB", 0, 0, 0, 0, 0, 1)
    data += b"perf-grid".ljust(20, b"\0") + bytes(256)
    for i in range(2):
        data += ("bone%d" % i).encode().ljust(20, b"\0")
    for i in range(10):
        data += ("toon%02d.bmp" % (i + 1)).encode().ljust(100, b"\0")
    data += pack("<II", 0, 0)
    pathlib.Path(path).write_bytes(data)


if __name__ == "__main__":
    generate(sys.argv[1], int(sys.argv[2]))
