# -*- coding: utf-8 -*-
# x64_disasm_case498.py - disassemble the two x64 bone-paste functions and
# their callers, resolve rip-relative operands (strings / globals / IAT).
# usage: python tools/x64_disasm_case498.py [path-to-exe]
import struct, sys
sys.stdout.reconfigure(encoding="utf-8")
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

EXE = sys.argv[1] if len(sys.argv) > 1 else r"MMD932/MikuMikudance.exe"
data = open(EXE, "rb").read()
pe = struct.unpack_from("<I", data, 0x3C)[0]
nsec = struct.unpack_from("<H", data, pe + 6)[0]
opt = struct.unpack_from("<H", data, pe + 20)[0]
imgbase = struct.unpack_from("<Q", data, pe + 24 + 24)[0]
secoff = pe + 24 + opt
secs = []
for i in range(nsec):
    s = secoff + i * 40
    name = data[s:s+8].split(b"\x00")[0].decode("latin1")
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

# --- IAT resolution from the import directory ---
imp_rva, imp_size = struct.unpack_from("<II", data, pe + 24 + 112 + 8)
iat = {}
for name, va_s, vs, rp, rs in secs:
    if va_s <= imp_rva < va_s + vs:
        off = rp + (imp_rva - va_s)
        while True:
            oft, ts, fc, name_rva, ft = struct.unpack_from("<IIIII", data, off)
            if oft == 0 and ft == 0:
                break
            dll = data[fileoff(imgbase + name_rva):].split(b"\x00")[0].decode()
            ilt_rva = oft or ft
            th_off = fileoff(imgbase + ilt_rva)
            iat_off = fileoff(imgbase + ft)
            k = 0
            while True:
                th = struct.unpack_from("<Q", data, th_off + k * 8)[0]
                if th == 0:
                    break
                slot_va = imgbase + ft + k * 8
                if th >> 63:
                    fname = f"ord#{th & 0xFFFF}"
                else:
                    fname = data[fileoff(imgbase + th + 2):].split(b"\x00")[0].decode()
                iat[slot_va] = f"{dll}!{fname}"
                k += 1
            off += 20
        break

def cstr(va, maxlen=24):
    fo = fileoff(va)
    if fo is None:
        return ""
    raw = data[fo:fo+maxlen].split(b"\x00")[0]
    try:
        return raw.decode("cp932")
    except Exception:
        return raw.hex()

def annotate(ins):
    """resolve rip-relative memory operand -> comment"""
    m = ins.op_str
    base = ins.address + ins.size
    out = []
    for tok in ["rip + 0x", "rip - 0x", "rip+0x", "rip-0x"]:
        if tok in m:
            rest = m.split(tok, 1)[1]
            num = rest.split("]")[0].strip()
            disp = int(num, 16)
            tgt = base + disp
            if tok.startswith("rip -") or tok == "rip-0x":
                tgt = base - disp
            if tgt in iat:
                out.append(f"IAT {iat[tgt]}")
            else:
                sn = secname(tgt)
                fo2 = fileoff(tgt)
                raw = data[fo2:fo2+20].split(b"\x00")[0] if fo2 is not None else b""
                if sn == ".rdata" and raw and all(0x20 <= b < 0x7F or b >= 0x80 for b in raw[:16]):
                    try:
                        s = raw.decode("cp932")
                        if any(ord(c) > 127 for c in s) or (s.isprintable() and len(s) >= 2):
                            out.append(f'"{s}" @0x{tgt:X}')
                            break
                    except Exception:
                        pass
                out.append(f"mem 0x{tgt:X}[{sn}]")
            break
    return "; ".join(out)

md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = False

def disasm(va, nbytes):
    fo = fileoff(va)
    code = data[fo:fo+nbytes]
    print(f"==== disasm 0x{va:X} .. 0x{va+nbytes:X} ====")
    for ins in md.disasm(code, va):
        ann = annotate(ins)
        print(f"  0x{ins.address:X}: {ins.mnemonic:8s} {ins.op_str:38s} {ann}")

disasm(0x140043F60, 0xA20)
disasm(0x1400C9DDF, 0x820)

# --- callers ---
print()
print("==== callers (call rel32 / call [rip]) ====")
def xrefs(tgt_va):
    out = []
    p = 0
    text = next(s for s in secs if s[0] == ".text")
    t_off, t_va, t_size = text[3], text[1], text[2]
    # direct call rel32: E8 disp32 where next+disp == tgt
    end = t_off + t_size - 5
    while True:
        i = data.find(b"\xe8", p, end)
        if i < 0:
            break
        rel = struct.unpack_from("<i", data, i + 1)[0]
        nxt = imgbase + t_va + (i + 5 - t_off)
        if nxt + rel == tgt_va:
            out.append(imgbase + t_va + (i - t_off))
        p = i + 1
    return out
for f in (0x140044033, 0x1400C9DDF):
    xs = xrefs(f)
    print(f"  0x{f:X} <- {len(xs)} caller(s): " + ", ".join(f"0x{x:X}" for x in xs))
    for x in xs:
        print(f"    caller context @0x{x:X}:")
        fo = fileoff(x)
        disasm(x - 0x40, 0xA0)
