# -*- coding: utf-8 -*-
# x64_find_consts.py - locate the 2.0f/30.0f/0.1f/... float pools and the
# camera-view look-at builder in the 64-bit MikuMikuDance reference image.
import struct
import sys

sys.stdout.reconfigure(encoding="utf-8")

import capstone

EXE = sys.argv[1] if len(sys.argv) > 1 else r"refs\MikuMikudance 926 CHS\MikuMikudance.exe"
data = open(EXE, "rb").read()
pe = struct.unpack_from("<I", data, 0x3C)[0]
nsec = struct.unpack_from("<H", data, pe + 6)[0]
opt = struct.unpack_from("<H", data, pe + 20)[0]
magic = struct.unpack_from("<H", data, pe + 24)[0]
if magic == 0x20B:
    imgbase = struct.unpack_from("<Q", data, pe + 24 + 24)[0]
else:
    imgbase = struct.unpack_from("<I", data, pe + 24 + 28)[0]
secoff = pe + 24 + opt
secs = []
for i in range(nsec):
    s = secoff + i * 40
    name = data[s:s + 8].split(b"\x00")[0].decode("latin1")
    vs, va, rs, rp = struct.unpack_from("<IIII", data, s + 8)
    secs.append((name, va, vs, rp, rs))
print("base 0x%X magic 0x%X" % (imgbase, magic))


def secname(va):
    rva = va - imgbase
    for name, va_s, vs, rp, rs in secs:
        if va_s <= rva < va_s + vs:
            return name
    return "?"


def fileoff(va):
    rva = va - imgbase
    for name, va_s, vs, rp, rs in secs:
        if va_s <= rva < va_s + vs:
            return rp + (rva - va_s)
    return None


def off2va(off):
    for name, va_s, vs, rp, rs in secs:
        if rp <= off < rp + rs:
            return imgbase + va_s + (off - rp)
    return None


def findall(pat, start=0, end=None):
    out = []
    i = start
    end = len(data) if end is None else end
    while True:
        i = data.find(pat, i, end)
        if i < 0:
            return out
        out.append(i)
        i += 1


rdata = next(s for s in secs if s[0] == ".rdata")
rd_off, rd_va, rd_size = rdata[3], imgbase + rdata[1], rdata[2]
text = next(s for s in secs if s[0] == ".text")
t_off, t_va, t_size = text[3], imgbase + text[1], text[2]

print("\n== float pool scan in .rdata ==")
wanted = {
    "0.3f": 0x3E99999A,
    "10.0f": 0x41200000,
    "1.0f": 0x3F800000,
    "0.1f": 0x3DCCCCCD,
    "0.4f": 0x3ECCCCCD,
    "0.01f": 0x3C23D70A,
    "80.0f": 0x42A00000,
    "45.0f": 0x42340000,
    "30.0f": 0x41F00000,
    "2.0f": 0x40000000,
}
for label, bits in wanted.items():
    pat = struct.pack("<I", bits)
    hits = [off2va(h) for h in findall(pat, rd_off, rd_off + rd_size)]
    print("  %-8s %08X -> %d hits: %s" % (label, bits, len(hits),
          ", ".join("0x%X" % h for h in hits[:12])))

# consecutive triple 0.3,10,1
trip = struct.pack("<III", 0x3E99999A, 0x41200000, 0x3F800000)
print("\ntriple 0.3f,10.0f,1.0f:", ["0x%X" % off2va(h) for h in findall(trip)])
trip2 = struct.pack("<III", 0x3E99999A, 0x41200000, 0x40000000)
print("triple 0.3f,10.0f,2.0f:", ["0x%X" % off2va(h) for h in findall(trip2)])
