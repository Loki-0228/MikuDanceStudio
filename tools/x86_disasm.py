# -*- coding: utf-8 -*-
# x86_disasm.py - linear disassembly helper for the 32-bit MMD 9.32 reference
# image.  Prints instruction-by-instruction listing for a VA range so the
# behaviour-level port can be checked against the binary.
#
# usage:
#   python tools/x86_disasm.py <exe> startVA endVA [--bytes N]
#   python tools/x86_disasm.py <exe> --find-hex "D9 05 ?? ?? ?? ??"
#   python tools/x86_disasm.py <exe> --str "camera"     (cp932/ascii strings)
import re
import struct
import sys

sys.stdout.reconfigure(encoding="utf-8")

import capstone


class Image:
    def __init__(self, path):
        self.data = open(path, "rb").read()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        self.pe = pe
        nsec = struct.unpack_from("<H", self.data, pe + 6)[0]
        opt = struct.unpack_from("<H", self.data, pe + 20)[0]
        self.magic = struct.unpack_from("<H", self.data, pe + 24)[0]
        if self.magic == 0x20B:
            self.base = struct.unpack_from("<Q", self.data, pe + 24 + 24)[0]
        else:
            self.base = struct.unpack_from("<I", self.data, pe + 24 + 28)[0]
        secoff = pe + 24 + opt
        self.secs = []
        for i in range(nsec):
            s = secoff + i * 40
            name = self.data[s:s + 8].split(b"\x00")[0].decode("latin1")
            vs, va, rs, rp = struct.unpack_from("<IIII", self.data, s + 8)
            self.secs.append((name, va, vs, rp, rs))

    def rva(self, va):
        return va - self.base

    def fileoff(self, va):
        rva = self.rva(va)
        for name, va_s, vs, rp, rs in self.secs:
            if va_s <= rva < va_s + vs:
                return rp + (rva - va_s)
        return None

    def off2va(self, off):
        for name, va_s, vs, rp, rs in self.secs:
            if rp <= off < rp + rs:
                return self.base + va_s + (off - rp)
        return None

    def secname(self, va):
        rva = self.rva(va)
        for name, va_s, vs, rp, rs in self.secs:
            if va_s <= rva < va_s + vs:
                return name
        return "?"

    def bytes_at(self, va, n):
        off = self.fileoff(va)
        if off is None:
            return b""
        return self.data[off:off + n]


def make_md(img):
    md = capstone.Cs(capstone.CS_ARCH_X86,
                     capstone.CS_MODE_32 if img.magic == 0x10B
                     else capstone.CS_MODE_64)
    md.detail = True
    return md


def disasm(img, start, end, context_before=0):
    md = make_md(img)
    off = img.fileoff(start)
    if off is None:
        print(f"0x{start:X} not mapped")
        return
    code = img.data[off:off + (end - start)]
    for ins in md.disasm(code, start):
        line = f"0x{ins.address:08X}  {ins.bytes.hex(' '):<24s} {ins.mnemonic} {ins.op_str}"
        ann = ""
        # annotate rip/absolute memory operands that land in a data section
        for m in re.finditer(r"0x([0-9a-fA-F]{6,16})", ins.op_str):
            tgt = int(m.group(1), 16)
            sec = img.secname(tgt)
            if sec != "?":
                ann += f"  ; -> {sec}+0x{img.rva(tgt):X}"
        print(line + ann)


def find_hex(img, pattern):
    parts = pattern.split()
    rx = b""
    for p in parts:
        if p in ("??", "?"):
            rx += b"."
        else:
            rx += re.escape(bytes([int(p, 16)]))
    for m in re.finditer(rx, img.data, re.S):
        va = img.off2va(m.start())
        print(f"file 0x{m.start():X}  VA 0x{va:X} [{img.secname(va)}]  "
              f"{img.data[m.start():m.start()+len(parts)].hex(' ')}")


def find_str(img, needle):
    for enc in ("cp932", "ascii", "utf-16-le"):
        try:
            pat = needle.encode(enc)
        except Exception:
            continue
        for m in re.finditer(re.escape(pat), img.data):
            va = img.off2va(m.start())
            print(f"[{enc}] file 0x{m.start():X} VA 0x{va:X} [{img.secname(va)}]")


def main():
    path = sys.argv[1]
    img = Image(path)
    print(f"magic=0x{img.magic:X} base=0x{img.base:X} sections={len(img.secs)}")
    for name, va, vs, rp, rs in img.secs:
        print(f"  {name:8s} VA 0x{img.base+va:08X} vsize 0x{vs:X} "
              f"file 0x{rp:X}")
    args = sys.argv[2:]
    if args and args[0] == "--find-hex":
        find_hex(img, args[1])
        return
    if args and args[0] == "--str":
        find_str(img, args[1])
        return
    start = int(args[0], 16)
    end = int(args[1], 16)
    disasm(img, start, end)


if __name__ == "__main__":
    main()
