# -*- coding: utf-8 -*-
# x64_disasm.py - disassemble a VA range of the 64-bit MMD reference image.
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


def fileoff(va):
    rva = va - imgbase
    for name, va_s, vs, rp, rs in secs:
        if va_s <= rva < va_s + vs:
            return rp + (rva - va_s)
    return None


def secname(va):
    rva = va - imgbase
    for name, va_s, vs, rp, rs in secs:
        if va_s <= rva < va_s + vs:
            return name
    return "?"


def fmt_float(v):
    if v == 0:
        return "0.0"
    return "%.6g" % v


start = int(sys.argv[1], 16)
end = int(sys.argv[2], 16)
off = fileoff(start)
code = data[off:off + (end - start)]
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.detail = True
for ins in md.disasm(code, start):
    ann = ""
    # annotate all absolute targets that fall in a section
    import re
    for m in re.finditer(r"0x([0-9a-f]{6,16})", ins.op_str):
        t = int(m.group(1), 16)
        s = secname(t)
        if s != "?":
            fo = fileoff(t)
            extra = ""
            if s == ".rdata" and fo is not None and t % 16 < 13:
                f = struct.unpack_from("<f", data, fo)[0]
                d = struct.unpack_from("<d", data, fo)[0]
                extra = " f=%s d=%s" % (fmt_float(f), fmt_float(d))
            ann += "  ; %s+0x%X%s" % (s, t - imgbase, extra)
    # FLD/FSTP style not needed for x64; annotate rip-relative resolved above
    print("0x%X  %-30s %s %s%s" % (ins.address, ins.bytes.hex(" "),
                                   ins.mnemonic, ins.op_str, ann))
