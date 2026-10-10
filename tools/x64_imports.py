# -*- coding: utf-8 -*-
# x64_imports.py - dump the 64-bit reference image's import table (DLL +
# function names) and, optionally, resolve a VA that points at an IAT slot.
import struct
import sys

sys.stdout.reconfigure(encoding="utf-8")

EXE = r"refs\MikuMikudance 926 CHS\MikuMikudance.exe"
data = open(EXE, "rb").read()
pe = struct.unpack_from("<I", data, 0x3C)[0]
nsec = struct.unpack_from("<H", data, pe + 6)[0]
opt = struct.unpack_from("<H", data, pe + 20)[0]
imgbase = struct.unpack_from("<Q", data, pe + 24 + 24)[0]
secoff = pe + 24 + opt
secs = []
for i in range(nsec):
    s = secoff + i * 40
    name = data[s:s + 8].split(b"\x00")[0].decode("latin1")
    vs, va, rs, rp = struct.unpack_from("<IIII", data, s + 8)
    secs.append((name, va, vs, rp, rs))


def fileoff(va):
    rva = va - imgbase
    for name, va_s, vs, rp, rs in secs:
        if va_s <= rva < va_s + vs:
            return rp + (rva - va_s)
    return None


def read_cstr(off):
    end = data.find(b"\x00", off)
    return data[off:end].decode("latin1")


dd = pe + 24 + (112 if struct.unpack_from("<H", data, pe + 24)[0] == 0x20B else 96)
imp_rva, imp_size = struct.unpack_from("<II", data, dd + 8)
iat = {}
off = fileoff(imgbase + imp_rva)
i = 0
while True:
    ent = data[off + i * 20: off + (i + 1) * 20]
    oft, ts, fc, name_rva, first_thunk = struct.unpack("<IIIII", ent)
    if name_rva == 0:
        break
    dll = read_cstr(fileoff(imgbase + name_rva))
    thunk_rva = oft if oft else first_thunk
    toff = fileoff(imgbase + thunk_rva)
    k = 0
    while True:
        val = struct.unpack_from("<Q", data, toff + k * 8)[0]
        if val == 0:
            break
        slot_va = imgbase + first_thunk + k * 8
        if not (val & 0x8000000000000000):
            nm = read_cstr(fileoff(imgbase + val) + 2)
            iat[slot_va] = f"{dll}!{nm}"
        k += 1
    i += 1

wanted = [int(a, 16) for a in sys.argv[1:]]
if wanted:
    for w in wanted:
        print("0x%X -> %s" % (w, iat.get(w, "<not an IAT slot>")))
else:
    bydll = {}
    for slot, nm in iat.items():
        dll = nm.split("!")[0]
        bydll.setdefault(dll, []).append((slot, nm))
    for dll in sorted(bydll):
        print("== %s ==" % dll)
        for slot, nm in sorted(bydll[dll]):
            print("   0x%X  %s" % (slot, nm.split("!")[1]))
