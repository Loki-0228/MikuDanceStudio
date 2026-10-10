# -*- coding: utf-8 -*-
# probe_view.py - measure the reference MMD viewport geometry from a PNG:
# locate the green Y axis, the red X axis, the grid rows and the camera sprite
# marker, so "where is the world origin on screen" is a number rather than an
# eyeball estimate.
#
# usage: python tools/probe_view.py <png> [--viewport l,t,r,b]
import struct
import sys
import zlib

sys.stdout.reconfigure(encoding="utf-8")


def load_png(path):
    data = open(path, "rb").read()
    assert data[:8] == b"\x89PNG\r\n\x1a\n"
    pos = 8
    width = height = None
    idat = bytearray()
    while pos < len(data):
        length = struct.unpack_from(">I", data, pos)[0]
        tag = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        if tag == b"IHDR":
            width, height, depth, color = struct.unpack_from(">IIBB", chunk, 0)
            assert depth == 8 and color == 2, (depth, color)
        elif tag == b"IDAT":
            idat += chunk
        elif tag == b"IEND":
            break
        pos += 12 + length
    raw = zlib.decompress(bytes(idat))
    stride = width * 3
    rows = []
    prev = bytearray(stride)
    p = 0
    for y in range(height):
        f = raw[p]
        p += 1
        line = bytearray(raw[p:p + stride])
        p += stride
        if f == 1:
            for i in range(3, stride):
                line[i] = (line[i] + line[i - 3]) & 0xFF
        elif f == 2:
            for i in range(stride):
                line[i] = (line[i] + prev[i]) & 0xFF
        elif f == 3:
            for i in range(stride):
                a = line[i - 3] if i >= 3 else 0
                line[i] = (line[i] + ((a + prev[i]) >> 1)) & 0xFF
        elif f == 4:
            for i in range(stride):
                a = line[i - 3] if i >= 3 else 0
                b = prev[i]
                c = prev[i - 3] if i >= 3 else 0
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pr = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 0xFF
        rows.append(bytes(line))
        prev = line
    return width, height, rows


def px(rows, x, y):
    r = rows[y]
    return r[x * 3], r[x * 3 + 1], r[x * 3 + 2]


def main():
    path = sys.argv[1]
    if "--viewport" in sys.argv:
        l, t, r, b = (int(v) for v in sys.argv[sys.argv.index("--viewport") + 1].split(","))
    else:
        l, t, r, b = 0, 0, 10 ** 9, 10 ** 9
    width, height, rows = load_png(path)
    print("image %dx%d" % (width, height))

    # green axis: strong green, low red/blue
    green_cols = {}
    red_rows = {}
    pink = []
    for y in range(max(0, t), min(height, b)):
        for x in range(max(0, l), min(width, r)):
            cr, cg, cb = px(rows, x, y)
            if cg > 150 and cr < 120 and cb < 120:
                green_cols[x] = green_cols.get(x, 0) + 1
            if cr > 150 and cg < 100 and cb < 100:
                red_rows[y] = red_rows.get(y, 0) + 1
            if cr > 220 and 60 < cg < 140 and 120 < cb < 200:
                pink.append((x, y))
    best_green = sorted(green_cols.items(), key=lambda kv: -kv[1])[:3]
    best_red = sorted(red_rows.items(), key=lambda kv: -kv[1])[:3]
    print("green axis columns:", best_green)
    print("red axis rows:    ", best_red)
    if pink:
        xs = [p[0] for p in pink]
        ys = [p[1] for p in pink]
        cx = sum(xs) // len(xs)
        cy = sum(ys) // len(ys)
        print("camera marker centre: (%d, %d)  n=%d  bbox x[%d,%d] y[%d,%d]"
              % (cx, cy, len(pink), min(xs), max(xs), min(ys), max(ys)))

    # Grid: rows of light-grey lines on the dark viewport.  Report the median
    # brightness per row inside the viewport to find the floor edge.
    print("\nrow brightness profile (every 4 px, viewport columns only):")
    x_lo = max(0, l)
    x_hi = min(width, r)
    for y in range(max(0, t), min(height, b), 8):
        total = 0
        count = 0
        for x in range(x_lo, x_hi, 3):
            cr, cg, cb = px(rows, x, y)
            total += (cr + cg + cb) // 3
            count += 1
        avg = total // max(1, count)
        print("  y=%4d  avg=%3d  %s" % (y, avg, "#" * (avg // 6)))


if __name__ == "__main__":
    main()
