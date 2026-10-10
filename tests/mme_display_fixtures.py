"""Generate tiny, original PMX fixtures for the hidden real-MME tests."""
from pathlib import Path
import struct
import sys


def text(value):
    data = value.encode('utf-16-le')
    return struct.pack('<i', len(data)) + data


def fixture(alpha_order=False):
    data = bytearray(b'PMX ' + struct.pack('<f', 2.0) + bytes([8, 0, 0, 1, 1, 1, 1, 1, 1]))
    for name in ('表示モデル', 'Display Model', '', ''):
        data += text(name)
    data += struct.pack('<i', 3)
    for x, y in ((-2, 10), (2, 10), (0, 14)):
        data += struct.pack('<8fBBf', x, y, 0, 0, 0, -1, 0, 0, 0, 0, 1)
    materials = [('front', 'front', .5), ('rear', 'rear', 1)] if alpha_order else [('左腕', 'CustomArm', 1)]
    data += struct.pack('<i', 3 * len(materials)) + bytes([0, 1, 2]) * len(materials)
    data += struct.pack('<ii', 0, len(materials))  # textures, materials
    for native, english, alpha in materials:
        data += text(native) + text(english)
        data += struct.pack('<11fB5fbbBBB', 1, 1, 1, alpha, 0, 0, 0, 1, .5, .5, .5,
                            15, 0, 0, 0, 1, 0, -1, -1, 0, 1, 0)
        data += text('') + struct.pack('<i', 3)
    data += struct.pack('<i', 1) + text('センター') + text('center')
    data += struct.pack('<3fbiH3f', 0, 0, 0, -1, 0, 26, 0, 0, 0)
    data += struct.pack('<4i', 0, 0, 0, 0)  # morphs, frames, bodies, joints
    return data


if __name__ == '__main__':
    root = Path(sys.argv[1]).resolve()
    root.mkdir(parents=True, exist_ok=True)
    for name, alpha in (('bilingual.pmx', False), ('alpha-order.pmx', True)):
        (root / name).write_bytes(fixture(alpha))
        print(root / name)
