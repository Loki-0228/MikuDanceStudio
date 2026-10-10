# -*- coding: utf-8 -*-
# x64_xref.py - print RIP-relative and absolute xrefs to a VA in the 64-bit
# reference image, with a short disassembly window around each hit.
import struct
import sys

sys.stdout.reconfigure(encoding="utf-8")

import capstone

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
text = next(s for s in secs if s[0] == ".text")
t_off, t_va, t_size = text[3], imgbase + text[1], text[2]


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


md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.detail = True

targets = [int(a, 16) for a in sys.argv[1:]]
window = 0x40

for tgt in targets:
    print("=== xrefs to 0x%X ===" % tgt)
    hits = []
    p = t_off
    end = t_off + t_size - 4
    while p < end:
        rel = struct.unpack_from("<i", data, p)[0]
        base = t_va + (p + 4 - t_off)
        if base + rel == tgt:
            hits.append(t_va + (p - t_off))
        p += 1
    for h in hits:
        start = h - window
        off = fileoff(start)
        code = data[off:off + window * 2]
        print("--- xref site 0x%X ---" % h)
        for ins in md.disasm(code, start):
            mark = " <<<" if ins.address <= h < ins.address + ins.size else ""
            print("  0x%X  %-24s %s %s%s" % (ins.address, ins.bytes.hex(" "),
                  ins.mnemonic, ins.op_str, mark))
        print()
