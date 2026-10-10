# -*- coding: utf-8 -*-
# x64_store_scan.py - linear-scan the 64-bit reference .text for memory
# accesses to a displacement off r12 (the app pointer) inside a VA window,
# printing each access with a short disassembly context.  Used to recover
# camera-structure writes/reads that a whole-function reading would bury.
import re
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
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.detail = True

start = int(sys.argv[1], 16)
end = int(sys.argv[2], 16)
disp = int(sys.argv[3], 16)
reg = sys.argv[4] if len(sys.argv) > 4 else "r12"

off = t_off + (start - t_va)
code = data[off:off + (end - start)]
hits = []
for ins in md.disasm(code, start):
    pat = r"\[%s \+ 0x%x\]" % (reg, disp)
    if re.search(pat, ins.op_str):
        kind = "W" if ins.mnemonic.startswith(
            ("mov", "add", "sub", "inc", "dec", "or", "and", "xor", "mul",
             "div")) and re.search(r"\[%s \+ 0x%x\], " % (reg, disp), ins.op_str) else "R"
        hits.append((ins.address, kind, ins.mnemonic, ins.op_str))
for a, kind, mn, ops in hits:
    print("0x%X %s %s %s" % (a, kind, mn, ops))
print("total %d" % len(hits))
