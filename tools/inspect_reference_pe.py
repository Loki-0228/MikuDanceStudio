"""Read-only PE64 inspection (pefile + capstone); VA output assumes preferred base.

Examples:
  python tools/inspect_reference_pe.py program.exe summary
  python tools/inspect_reference_pe.py program.exe xrefs _vcomp_fork
  python tools/inspect_reference_pe.py program.exe function 0x140012340
  python tools/inspect_reference_pe.py program.exe function 0x140012340 0x140012400
  python tools/inspect_reference_pe.py program.exe rtti NndSkiningComputer
  python tools/inspect_reference_pe.py shader.cso shader
No target executable is loaded or executed. The shader command uses the system
D3DCompiler disassembler on the supplied DXBC bytes.
"""
import argparse
import bisect
import ctypes
import hashlib
import pathlib
import re
import struct
import sys

import capstone as cs
import pefile


def shader(path):
    data = path.read_bytes()
    dll = ctypes.WinDLL('d3dcompiler_47.dll')
    fn = dll.D3DDisassemble
    fn.argtypes = [ctypes.c_void_p, ctypes.c_size_t, ctypes.c_uint,
                   ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p)]
    fn.restype = ctypes.c_long
    blob = ctypes.c_void_p()
    buffer = ctypes.create_string_buffer(data)
    hr = fn(buffer, len(data), 0, None, ctypes.byref(blob))
    if hr < 0:
        raise RuntimeError(f'D3DDisassemble: {hr:#x}')
    table = ctypes.cast(blob, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
    try:
        ptr = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(table[3])(blob)
        size = ctypes.WINFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(table[4])(blob)
        print(ctypes.string_at(ptr, size).rstrip(b'\0').decode('utf-8'))
    finally:
        ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(table[2])(blob)


class Image:
    def __init__(self, path):
        self.path = path
        self.raw = path.read_bytes()
        self.pe = pefile.PE(data=self.raw)
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.imports = {i.address: e.dll.decode() + '!' +
                        (i.name.decode() if i.name else f'ordinal_{i.ordinal}')
                        for e in self.pe.DIRECTORY_ENTRY_IMPORT for i in e.imports}
        self.ranges = sorted((self.base + e.struct.BeginAddress,
                              self.base + e.struct.EndAddress)
                             for e in getattr(self.pe, 'DIRECTORY_ENTRY_EXCEPTION', []))
        self.starts = [a for a, b in self.ranges]
        self.md = cs.Cs(cs.CS_ARCH_X86, cs.CS_MODE_64)
        self.md.detail = True
        self.md.skipdata = True

    def owner(self, va):
        i = bisect.bisect_right(self.starts, va) - 1
        return self.ranges[i] if i >= 0 and va < self.ranges[i][1] else None

    def disasm(self, begin, end):
        return self.md.disasm(self.pe.get_data(begin - self.base, end - begin), begin)

    def references(self, ins):
        if not ins.id:
            return []
        return [ins.address + ins.size + o.mem.disp for o in ins.operands
                if o.type == cs.x86.X86_OP_MEM and o.mem.base == cs.x86.X86_REG_RIP]

    def line(self, ins):
        labels = [self.imports[a] for a in self.references(ins) if a in self.imports]
        return f'{ins.address:016x}  {ins.mnemonic:10} {ins.op_str}' + (
            ' ; ' + ', '.join(labels) if labels else '')

    def all_ins(self):
        for section in self.pe.sections:
            if section.Characteristics & 0x20000000:
                yield from self.md.disasm(section.get_data(), self.base + section.VirtualAddress)

    def xrefs(self, needle):
        targets = {a: name for a, name in self.imports.items() if needle.lower() in name.lower()}
        if needle.startswith('0x'):
            targets[int(needle, 16)] = 'address'
        for encoding in ['utf-8', 'utf-16-le']:
            for match in re.finditer(re.escape(needle.encode(encoding)), self.raw):
                va = self.base + self.pe.get_rva_from_offset(match.start())
                targets[va] = f'string ({encoding}) {needle}'
        print('targets', {hex(a): s for a, s in targets.items()})
        for ins in self.all_ins():
            refs = self.references(ins)
            if ins.id and ins.mnemonic in ('call', 'jmp'):
                refs += [o.imm for o in ins.operands if o.type == cs.x86.X86_OP_IMM]
            if any(a in targets for a in refs):
                owner = self.owner(ins.address)
                print(self.line(ins), 'function', tuple(hex(a) for a in owner) if owner else '?')

    def rtti(self, name):
        """Recover MSVC x64 complete-object locators and their vtables."""
        pattern = re.escape(('.?AV' + name + '@@').encode() + b'\0')
        for match in re.finditer(pattern, self.raw):
            descriptor = self.pe.get_rva_from_offset(match.start() - 16)
            for ref in re.finditer(re.escape(struct.pack('<I', descriptor)), self.raw):
                off = ref.start() - 12
                if off < 0 or off + 24 > len(self.raw):
                    continue
                sig, adjust, ctor, typ, hierarchy, self_rva = struct.unpack_from('<6I', self.raw, off)
                if sig != 1 or self_rva != self.pe.get_rva_from_offset(off):
                    continue
                locator = self.base + self_rva
                for ptr in re.finditer(re.escape(struct.pack('<Q', locator)), self.raw):
                    table = self.base + self.pe.get_rva_from_offset(ptr.start() + 8)
                    print(name, 'locator', hex(locator), 'vtable', hex(table), 'adjust', adjust)
                    for i in range(24):
                        address = struct.unpack('<Q', self.pe.get_data(table-self.base+i*8, 8))[0]
                        section = self.pe.get_section_by_rva(address-self.base)
                        if section is None or not section.Characteristics & 0x20000000:
                            break
                        print(f'  slot {i} offset {i*8:#x} -> {address:#x}')


def main():
    sys.stdout.reconfigure(encoding='utf-8')
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('path', type=pathlib.Path)
    ap.add_argument('mode', choices=['summary', 'xrefs', 'function', 'shader', 'rtti'])
    ap.add_argument('value', nargs='?')
    ap.add_argument('end', nargs='?', help='explicit exclusive end VA for a code range')
    args = ap.parse_args()
    if args.mode == 'shader':
        return shader(args.path)
    img = Image(args.path)
    if args.mode == 'summary':
        print('sha256', hashlib.sha256(img.raw).hexdigest())
        print('base', hex(img.base), 'machine', hex(img.pe.FILE_HEADER.Machine),
              'runtime functions', len(img.ranges))
        for a, name in img.imports.items():
            print(hex(a), name)
    elif args.mode == 'xrefs':
        img.xrefs(args.value)
    elif args.mode == 'rtti':
        img.rtti(args.value)
    else:
        va = int(args.value, 0)
        # An x64 runtime-function entry can describe just one chained unwind
        # fragment, not the entire source function. Explicit bounds permit
        # inspecting adjacent fragments after confirming instruction boundaries.
        region = (va, int(args.end, 0)) if args.end else img.owner(va)
        if region is None:
            raise ValueError('No runtime-function range for this address')
        print('range', tuple(hex(a) for a in region))
        for ins in img.disasm(*region):
            print(img.line(ins))


if __name__ == '__main__':
    main()
