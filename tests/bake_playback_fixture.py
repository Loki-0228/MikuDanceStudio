"""Generate a self-contained PMX stress scene for bake_playback_tests."""
import pathlib
import struct
import sys


def fixture(path, bone_count=400, triangles_per_bone=80):
    data = bytearray(b'PMX ')

    def put(fmt, *values):
        data.extend(struct.pack('<' + fmt, *values))

    def text(value):
        encoded = value.encode('utf-16-le')
        put('i', len(encoded))
        data.extend(encoded)

    put('f9B', 2., 8, 0, 0, 4, 4, 4, 4, 4, 4)
    for value in ('Bake diagnostic', 'Bake diagnostic', '', ''):
        text(value)
    positions = [((b // 4 % 10 - 4.5) * .8, 10 - b % 4 * .45,
                  (b // 40 - 4.5) * .8) for b in range(bone_count)]
    vertices = bone_count * triangles_per_bone * 3
    put('i', vertices)
    for b, (x, y, z) in enumerate(positions):
        for triangle in range(triangles_per_bone):
            # Small distinct triangles, not thousands of coincident surfaces.
            ox = (triangle % 10 - 4.5) * .025
            oy = (triangle // 10 - 3.5) * .025
            for dx, dy in ((0, 0), (.024, 0), (0, .024)):
                put('8fBif', x + ox + dx, y + oy + dy, z, 0, 0, -1,
                    0, 0, 0, b, 1)
    put('i', vertices)
    for i in range(vertices):
        put('i', i)
    put('ii', 0, 1)  # textures, materials
    text('Diagnostic'); text('Diagnostic')
    put('11fB5fiiBBB', .2, .6, 1, 1, 0, 0, 0, 1, .2, .2, .2,
        1, 0, 0, 0, 1, 0, -1, -1, 0, 1, 0)
    text(''); put('i', vertices)
    put('i', bone_count)
    for b, position in enumerate(positions):
        text(f'bone{b}'); text(f'bone{b}')
        put('3fiiH3f', *position, b-1 if b % 4 else -1, 0, 0x1e, 0, .2, 0)
    put('ii', 0, 1)  # morphs, display groups
    text('Bones'); text('Bones'); put('Bi', 0, bone_count)
    for b in range(bone_count):
        put('Bi', 0, b)
    put('i', bone_count)
    for b, position in enumerate(positions):
        text(f'body{b}'); text(f'body{b}')
        put('iBHB3f3f3f5fB', b, 0, 0, 0, .15, .15, .15,
            *position, 0, 0, 0, 1, .1, .1, 0, .5, 1 if b % 4 else 0)
    put('i', bone_count * 3 // 4)
    for b, (x, y, z) in enumerate(positions):
        if not b % 4:
            continue
        text(f'joint{b}'); text(f'joint{b}')
        put('Bii', 0, b-1, b)
        put('24f', x, y+.225, z, 0, 0, 0, 0, 0, 0, 0, 0, 0,
            -.3, -.3, -.3, .3, .3, .3, 0, 0, 0, .8, .8, .8)
    path = pathlib.Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)
    print(f'{path}: bones={bone_count}, rigid={bone_count}, '
          f'joints={bone_count * 3 // 4}, vertices={vertices}, bytes={len(data)}')


if __name__ == '__main__':
    fixture(sys.argv[1], triangles_per_bone=int(sys.argv[2]) if len(sys.argv)>2 else 80)
