# -*- coding: utf-8 -*-
# x64_scan.py - whole-.text linear disassembly index with a cache so repeated
# queries over the 64-bit MMD reference image are cheap.
#
# usage:
#   python tools/x64_scan.py writes 0x340 0x344 0x9f040 ...
#   python tools/x64_scan.py refs  0x140132980 0x140132984 ...
#   python tools/x64_scan.py blocks 0x140027700 0x140027720
import os
import pickle
import re
import struct
import sys

sys.stdout.reconfigure(encoding="utf-8")

import capstone

EXE = r"refs\MikuMikudance 926 CHS\MikuMikudance.exe"
CACHE = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                     ".x64_scan_cache.pkl")

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


def build():
    md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
    md.detail = True
    # The section carries embedded jump tables / data pools; without skipdata
    # the linear sweep stops at the first non-instruction byte.
    md.skipdata = True
    code = data[t_off:t_off + t_size]
    insns = []
    for ins in md.disasm(code, t_va):
        insns.append((ins.address, ins.bytes, ins.mnemonic, ins.op_str))
    return insns


if os.path.exists(CACHE):
    with open(CACHE, "rb") as fh:
        insns = pickle.load(fh)
else:
    insns = build()
    with open(CACHE, "wb") as fh:
        pickle.dump(insns, fh)
print("loaded %d instructions" % len(insns))

# index: absolute VA -> instructions whose text contains a +0xNNNN known
# displacement off r12.
mode = sys.argv[1]
md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
md.detail = True

if mode == "writes":
    for a in sys.argv[2:]:
        off = int(a, 16)
        rx = re.compile(r"\[r12 \+ 0x%x\]" % off)
        print("== writes/reads of app+0x%X ==" % off)
        for addr, byt, mn, ops in insns:
            if not rx.search(ops):
                continue
            # decode the first operand to see whether the memory is the dst
            try:
                dec = next(md.disasm(byt, addr))
            except StopIteration:
                continue
            if not dec.operands:
                continue
            dst = dec.operands[0]
            if dst.type == capstone.x86.X86_OP_MEM:
                print("  0x%X  %-8s %s" % (addr, mn, ops))
        print()
elif mode == "reads":
    for a in sys.argv[2:]:
        off = int(a, 16)
        rx = re.compile(r"\[r12 \+ 0x%x\]" % off)
        print("== reads of app+0x%X ==" % off)
        for addr, byt, mn, ops in insns:
            if not rx.search(ops):
                continue
            try:
                dec = next(md.disasm(byt, addr))
            except StopIteration:
                continue
            if not dec.operands:
                continue
            dst = dec.operands[0]
            if dst.type == capstone.x86.X86_OP_REG:
                print("  0x%X  %-8s %s" % (addr, mn, ops))
        print()
elif mode == "refs":
    for a in sys.argv[2:]:
        tgt = int(a, 16)
        print("== code refs to 0x%X ==" % tgt)
        for addr, byt, mn, ops in insns:
            try:
                dec = next(md.disasm(byt, addr))
            except StopIteration:
                continue
            found = False
            for op in dec.operands:
                if op.type == capstone.x86.X86_OP_MEM and \
                        op.mem.base == capstone.x86.X86_REG_RIP:
                    if dec.address + dec.size + op.mem.disp == tgt:
                        found = True
                elif op.type == capstone.x86.X86_OP_IMM and op.imm == tgt:
                    found = True
            if found or ("0x%x" % tgt) in ops.lower():
                print("  0x%X  %-10s %-24s %s" % (addr, mn, byt.hex(" "), ops))
        print()
elif mode == "const":
    # scan .rdata float pools for a value
    rdata = next(s for s in secs if s[0] == ".rdata")
    rd_off, rd_va, rd_size = rdata[3], imgbase + rdata[1], rdata[2]
    for a in sys.argv[2:]:
        v = float(a)
        pat = struct.pack("<f", v)
        i = rd_off
        print("== 0x%X (%.9g) in .rdata ==" % (struct.unpack("<I", pat)[0], v))
        while True:
            i = data.find(pat, i, rd_off + rd_size)
            if i < 0:
                break
            print("  0x%X" % (rd_va + (i - rd_off)))
            i += 1
        print()
