# -*- coding: utf-8 -*-
# x64_ref_scan.py - locate x64 reference-binary globals and their code xrefs.
#
# Scans the x64 MMD 9.32 reference image (MikuMikudance.exe, PE32+) for:
#   1. the six runtime scale/init globals (by exact float/double bit pattern)
#   2. the case-498 SJIS needle bytes and their xrefs
#   3. the startup extension strings (.PMM/.PMD/.PMX) and their xrefs
#   4. all dword mov-imm32 writes (C7 05 rel32 imm32) of 2.0f / 30.0f in .text
#      (the suspected runtime initializers for g_QuatScaleFactor / g_FrameScale)
#
# usage: python tools/x64_ref_scan.py [path-to-exe]
import struct, sys, hashlib
sys.stdout.reconfigure(encoding="utf-8")

EXE = sys.argv[1] if len(sys.argv) > 1 else r"MMD932/MikuMikudance.exe"
data = open(EXE, "rb").read()

pe = struct.unpack_from("<I", data, 0x3C)[0]
nsec = struct.unpack_from("<H", data, pe + 6)[0]
opt = struct.unpack_from("<H", data, pe + 20)[0]
magic = struct.unpack_from("<H", data, pe + 24)[0]
imgbase = struct.unpack_from("<Q", data, pe + 24 + 24)[0]
secoff = pe + 24 + opt
secs = []
for i in range(nsec):
    s = secoff + i * 40
    name = data[s:s+8].split(b"\x00")[0].decode("latin1")
    vs, va, rs, rp = struct.unpack_from("<IIII", data, s + 8)
    secs.append((name, va, vs, rp, rs))
print(f"image: magic=0x{magic:X} base=0x{imgbase:X} sections={nsec}")
print(f"sha256: {hashlib.sha256(data).hexdigest().upper()}")

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

def findall(pat):
    out, i = [], 0
    while True:
        i = data.find(pat, i)
        if i < 0:
            return out
        out.append(i)
        i += 1

def off2va(off):
    for name, va_s, vs, rp, rs in secs:
        if rp <= off < rp + rs:
            return imgbase + va_s + (off - rp)
    return None

text = next(s for s in secs if s[0] == ".text")
text_off, text_va, text_size = text[3], imgbase + text[1], text[2]

print()
print("== 1) constant-pattern hits (section, VA, file-off) ==")
consts = {
    "MouseScaleA dbl 0.5": struct.pack("<d", 0.5),
    "MouseScaleA flt 0.5": struct.pack("<f", 0.5),
    "MouseScaleB dbl 0.004999999888241291": struct.pack("<d", 0.004999999888241291),
    "MouseScaleB flt": struct.pack("<f", 0.004999999888241291),
    "MouseScaleB dbl 0.005 true": struct.pack("<d", 0.005),
    "MouseScaleC dbl 0.05000000074505806": struct.pack("<d", 0.05000000074505806),
    "MouseScaleC flt": struct.pack("<f", 0.05000000074505806),
    "MouseScaleC dbl 0.05 true": struct.pack("<d", 0.05),
    "MouseScaleD flt 0.0020000000949949": struct.pack("<f", 0.0020000000949949),
    "double 30.0 (qword pool)": struct.pack("<d", 30.0),
    "float 30.0 (dword pool)": struct.pack("<f", 30.0),
    "double 100.0 (qword pool)": struct.pack("<d", 100.0),
    "float 2.0 (dword pool)": struct.pack("<f", 2.0),
    "float 0.01 (old guess)": struct.pack("<f", 0.01),
}
for label, pat in consts.items():
    hits = findall(pat)
    vas = []
    for h in hits:
        va = off2va(h)
        if va is not None:
            vas.append(f"0x{va:X}[{secname(va)}]")
    print(f"  {label:36s} {pat.hex(' ')} -> {len(hits)} hit(s): {', '.join(vas[:6])}")

print()
print("== 2) SJIS needles (case 498) ==")
needles = {
    "kNeedle530BF8 8945": bytes([0x89, 0x45]),
    "kNeedle530BFC モデル削除": bytes([0x83,0x82,0x83,0x66,0x83,0x8B,0x8D,0xED,0x8F,0x9C]),
    "ボーン削除 compare": bytes([0x83,0x7B,0x81,0x5B,0x83,0x93,0x8D,0xED,0x8F,0x9C]),
}
for label, pat in needles.items():
    hits = findall(pat)
    vas = [f"0x{off2va(h):X}" for h in hits if off2va(h) is not None]
    print(f"  {label:28s} -> {len(hits)} hit(s): {', '.join(vas[:8])}")

print()
print("== 3) startup extension strings ==")
for label, pat in {
    "wide .PMM": ".PMM".encode("utf-16-le"),
    "wide .PMD": ".PMD".encode("utf-16-le"),
    "wide .PMX": ".PMX".encode("utf-16-le"),
    "ascii .PMM": b".PMM",
    "ascii .PMD": b".PMD",
    "ascii .PMX": b".PMX",
}.items():
    hits = findall(pat)
    vas = [f"0x{off2va(h):X}" for h in hits if off2va(h) is not None]
    print(f"  {label:12s} -> {len(hits)} hit(s): {', '.join(vas[:8])}")

print()
print("== 4) C7 05 rel32 imm32 writers of 2.0f / 30.0f in .text ==")
targets = {}
def addtarget(va, label):
    if va: targets[va] = targets.get(va, label)
for label, pat in consts.items():
    for h in findall(pat):
        va = off2va(h)
        if va: addtarget(va, label)
for label, pat in needles.items():
    for h in findall(pat):
        va = off2va(h)
        if va: addtarget(va, label)

WANTED_F32 = {0x40000000: "2.0f", 0x41F00000: "30.0f"}
writers = []
i = 0
while True:
    i = data.find(b"\xc7\x05", i)
    if i < 0: break
    if text_off <= i < text_off + text_size - 10:
        rel = struct.unpack_from("<i", data, i + 2)[0]
        imm = struct.unpack_from("<I", data, i + 6)[0]
        instr_end_va = text_va + (i + 6 - text_off)
        tgt = instr_end_va + rel
        if imm in WANTED_F32:
            writers.append((i, tgt, imm))
    i += 1
for off, tgt, imm in writers:
    va = text_va + (off - text_off)
    print(f"  mov [0x{tgt:X}] = {WANTED_F32[imm]} @ 0x{va:X} (file 0x{off:X})")

print()
print("== 5) rip-relative xrefs to the found constants/needles/strings ==")
refs = {}
p = text_off
end = text_off + text_size - 4
while p < end:
    rel = struct.unpack_from("<i", data, p)[0]
    base = text_va + (p + 4 - text_off)
    tgt = base + rel
    if tgt in targets:
        refs.setdefault(tgt, []).append(p)
    p += 1
for tgt, ps in sorted(refs.items()):
    vas = [f"0x{text_va + (q - text_off):X}" for q in ps]
    print(f"  0x{tgt:X} ({targets[tgt]}) <- {len(ps)} xref(s): {', '.join(vas[:10])}")

print()
print("== 6) context hexdump at first xref of each target ==")
for tgt, ps in sorted(refs.items()):
    if not ps: continue
    va = text_va + (ps[0] - text_off)
    fo = fileoff(va)
    print(f"  @0x{va:X} ({targets[tgt]}):")
    print("    " + data[fo-8:fo+24].hex(" "))
# --- extended pass: rdata-only needles, ext-string xrefs, pool dump, movss stores ---
rdata = next(s for s in secs if s[0] == ".rdata")
rd_off, rd_va, rd_size = rdata[3], rdata[1], rdata[2]

print()
print("== 2b) .rdata-only needle searches ==")
for label, pat in {
    "89 45 00 (nul-terminated)": bytes([0x89, 0x45, 0x00]),
    "shoujo 8DED8F9C": bytes([0x8D, 0xED, 0x8F, 0x9C]),
    "89 45 00 00": bytes([0x89, 0x45, 0x00, 0x00]),
}.items():
    hits = []
    for h in findall(pat):
        if rd_off <= h < rd_off + rd_size:
            hits.append(h)
    print(f"  {label:28s} -> {len(hits)} hit(s): " + ", ".join(f"0x{off2va(h):X}" for h in hits[:10]))

print()
print("== 2c) cp932 decode around 0x14012D9B0 ==")
fo = fileoff(0x14012D9B0)
print("  " + data[fo-16:fo+48].decode("cp932", errors="replace"))

print()
print("== 1b) float pool dump 0x1401329C0..0x140132D80 ==")
base = fileoff(0x1401329C0)
for row in range(0, 0x3C0, 16):
    va = 0x1401329C0 + row
    vals = []
    for k in range(0, 16, 4):
        f = struct.unpack_from("<f", data, base + row + k)[0]
        vals.append(f"{f: .6g}")
    print(f"  0x{va:X}: " + "  ".join(vals))

print()
print("== 4b) movss stores to .data (F3 0F 11 /0 disp32) ==")
stores = []
i = text_off
while True:
    i = data.find(b"\xf3\x0f\x11", i)
    if i < 0: break
    if i + 8 <= text_off + text_size:
        modrm = data[i + 3]
        if (modrm & 0xC7) == 0x05:
            rel = struct.unpack_from("<i", data, i + 4)[0]
            tgt = text_va + (i + 8 - text_off) + rel
            if secname(tgt) == ".data":
                stores.append((i, tgt))
    i += 1
for off, tgt in stores:
    va = text_va + (off - text_off)
    fov = fileoff(tgt)
    f = struct.unpack_from("<f", data, fov)[0] if fov is not None else float("nan")
    print(f"  movss [0x{tgt:X}] = xmm @ 0x{va:X}  (file-image value {f:.6g})")

print()
print("== 4c) movsd stores to .data (F2 0F 11 /0 disp32) ==")
stores2 = []
i = text_off
while True:
    i = data.find(b"\xf2\x0f\x11", i)
    if i < 0: break
    if i + 8 <= text_off + text_size:
        modrm = data[i + 3]
        if (modrm & 0xC7) == 0x05:
            rel = struct.unpack_from("<i", data, i + 4)[0]
            tgt = text_va + (i + 8 - text_off) + rel
            if secname(tgt) == ".data":
                stores2.append((i, tgt))
    i += 1
for off, tgt in stores2:
    va = text_va + (off - text_off)
    print(f"  movsd [0x{tgt:X}] = xmm @ 0x{va:X}")

EXT = {
    "wide .PMM": ".PMM".encode("utf-16-le"),
    "wide .PMD": ".PMD".encode("utf-16-le"),
    "wide .PMX": ".PMX".encode("utf-16-le"),
}
for label, pat in EXT.items():
    for h in findall(pat):
        va = off2va(h)
        if va: addtarget(va, label)

print()
print("== 5b) xrefs to ext strings and mouse pool elements ==")
refs2 = {}
p = text_off
while p < end:
    rel = struct.unpack_from("<i", data, p)[0]
    base = text_va + (p + 4 - text_off)
    tgt = base + rel
    if tgt in targets and (targets[tgt].startswith("wide .") or 0x1401329C0 <= tgt <= 0x140132D80):
        refs2.setdefault(tgt, []).append(p)
    p += 1
for tgt, ps in sorted(refs2.items()):
    vas = [f"0x{text_va + (q - text_off):X}" for q in ps]
    print(f"  0x{tgt:X} ({targets[tgt]}) <- {', '.join(vas[:8])}")

